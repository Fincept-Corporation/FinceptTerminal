/**
 * Deposit Panel
 * Shows deposit address and QR code for receiving crypto
 */

import React, { useState } from 'react';
import { Copy, Check, RefreshCw, AlertCircle, QrCode } from 'lucide-react';
import { useDepositAddress } from '../../hooks/useFunding';
import { useBalance } from '../../hooks/useAccountInfo';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

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
  YELLOW: '#FFD700',
  CYAN: '#00E5FF',
};

// Common networks for deposits
const NETWORKS: Record<string, string[]> = {
  BTC: ['BTC', 'BEP20', 'BEP2'],
  ETH: ['ERC20', 'BEP20', 'ARBITRUM', 'OPTIMISM', 'POLYGON'],
  USDT: ['ERC20', 'TRC20', 'BEP20', 'SOL', 'POLYGON'],
  USDC: ['ERC20', 'SOL', 'BEP20', 'POLYGON', 'ARBITRUM'],
  SOL: ['SOL'],
  XRP: ['XRP'],
  DOGE: ['DOGE', 'BEP20'],
  ADA: ['ADA', 'BEP20'],
};

export function DepositPanel() {
  const { tradingMode, activeBroker } = useBrokerContext();
  const { balances } = useBalance(false);
  const [selectedCurrency, setSelectedCurrency] = useState<string>('');
  const [selectedNetwork, setSelectedNetwork] = useState<string>('');
  const [copied, setCopied] = useState<'address' | 'tag' | null>(null);

  const { address, isLoading, error, isSupported, refresh } = useDepositAddress(
    selectedCurrency || undefined,
    selectedNetwork || undefined
  );

  // Get unique currencies from balances
  const availableCurrencies = Array.from(new Set(balances.map((b) => b.currency))).sort();

  const handleCurrencyChange = (currency: string) => {
    setSelectedCurrency(currency);
    setSelectedNetwork(''); // Reset network when currency changes
  };

  const copyToClipboard = (text: string, type: 'address' | 'tag') => {
    navigator.clipboard.writeText(text);
    setCopied(type);
    setTimeout(() => setCopied(null), 2000);
  };

  if (tradingMode === 'paper') {
    return (
      <div
        style={{
          padding: '40px',
          textAlign: 'center',
          color: FINCEPT.GRAY,
          background: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '4px',
        }}
      >
        <AlertCircle size={32} color={FINCEPT.YELLOW} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px', marginBottom: '8px' }}>
          Deposits not available in paper trading mode
        </div>
        <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
          Connect a live exchange to deposit funds
        </div>
      </div>
    );
  }

  return (
    <div
      style={{
        background: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '4px',
        padding: '16px',
      }}
    >
      {/* Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          marginBottom: '16px',
          paddingBottom: '12px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}
      >
        <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>
          Deposit to {activeBroker?.charAt(0).toUpperCase()}{activeBroker?.slice(1)}
        </div>
      </div>

      {/* Currency Selection */}
      <div style={{ marginBottom: '16px' }}>
        <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px' }}>
          SELECT CURRENCY
        </label>
        <select
          value={selectedCurrency}
          onChange={(e) => handleCurrencyChange(e.target.value)}
          style={{
            width: '100%',
            padding: '10px 12px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px',
            color: FINCEPT.WHITE,
            fontSize: '12px',
            cursor: 'pointer',
          }}
        >
          <option value="">-- Select Currency --</option>
          {availableCurrencies.map((currency) => (
            <option key={currency} value={currency}>
              {currency}
            </option>
          ))}
          {/* Common currencies if not in balance */}
          {!availableCurrencies.includes('BTC') && <option value="BTC">BTC</option>}
          {!availableCurrencies.includes('ETH') && <option value="ETH">ETH</option>}
          {!availableCurrencies.includes('USDT') && <option value="USDT">USDT</option>}
          {!availableCurrencies.includes('USDC') && <option value="USDC">USDC</option>}
        </select>
      </div>

      {/* Network Selection */}
      {selectedCurrency && NETWORKS[selectedCurrency] && (
        <div style={{ marginBottom: '16px' }}>
          <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px' }}>
            SELECT NETWORK
          </label>
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
            {NETWORKS[selectedCurrency].map((network) => (
              <button
                key={network}
                onClick={() => setSelectedNetwork(network)}
                style={{
                  padding: '6px 12px',
                  backgroundColor: selectedNetwork === network ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                  border: `1px solid ${selectedNetwork === network ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderRadius: '4px',
                  color: selectedNetwork === network ? FINCEPT.WHITE : FINCEPT.GRAY,
                  fontSize: '10px',
                  cursor: 'pointer',
                  fontWeight: selectedNetwork === network ? 700 : 400,
                }}
              >
                {network}
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Deposit Address Display */}
      {selectedCurrency && (
        <div style={{ marginTop: '20px' }}>
          {isLoading ? (
            <div
              style={{
                padding: '40px',
                textAlign: 'center',
                color: FINCEPT.GRAY,
              }}
            >
              <RefreshCw
                size={24}
                color={FINCEPT.ORANGE}
                style={{ animation: 'spin 1s linear infinite', marginBottom: '12px' }}
              />
              <div style={{ fontSize: '11px' }}>Loading deposit address...</div>
            </div>
          ) : error || !address ? (
            <div
              style={{
                padding: '30px',
                textAlign: 'center',
                backgroundColor: `${FINCEPT.RED}10`,
                borderRadius: '4px',
              }}
            >
              <AlertCircle size={24} color={FINCEPT.RED} style={{ marginBottom: '8px' }} />
              <div style={{ fontSize: '11px', color: FINCEPT.RED, marginBottom: '8px' }}>
                {error?.message || 'Failed to generate deposit address'}
              </div>
              <button
                onClick={refresh}
                style={{
                  padding: '6px 12px',
                  backgroundColor: FINCEPT.ORANGE,
                  border: 'none',
                  borderRadius: '4px',
                  color: FINCEPT.WHITE,
                  fontSize: '10px',
                  cursor: 'pointer',
                }}
              >
                Try Again
              </button>
            </div>
          ) : (
            <div>
              {/* Address Box */}
              <div
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '4px',
                  padding: '16px',
                  marginBottom: '12px',
                }}
              >
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
                  {selectedCurrency} DEPOSIT ADDRESS
                  {address.network && ` (${address.network})`}
                </div>
                <div
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                  }}
                >
                  <div
                    style={{
                      flex: 1,
                      padding: '12px',
                      backgroundColor: FINCEPT.PANEL_BG,
                      borderRadius: '4px',
                      fontFamily: '"IBM Plex Mono", monospace',
                      fontSize: '11px',
                      color: FINCEPT.CYAN,
                      wordBreak: 'break-all',
                    }}
                  >
                    {address.address}
                  </div>
                  <button
                    onClick={() => copyToClipboard(address.address, 'address')}
                    style={{
                      padding: '12px',
                      backgroundColor: copied === 'address' ? FINCEPT.GREEN : FINCEPT.ORANGE,
                      border: 'none',
                      borderRadius: '4px',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                    }}
                    title="Copy address"
                  >
                    {copied === 'address' ? (
                      <Check size={16} color={FINCEPT.WHITE} />
                    ) : (
                      <Copy size={16} color={FINCEPT.WHITE} />
                    )}
                  </button>
                </div>
              </div>

              {/* Tag/Memo (if applicable) */}
              {address.tag && (
                <div
                  style={{
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.YELLOW}`,
                    borderRadius: '4px',
                    padding: '16px',
                    marginBottom: '12px',
                  }}
                >
                  <div
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      marginBottom: '8px',
                    }}
                  >
                    <AlertCircle size={12} color={FINCEPT.YELLOW} />
                    <span style={{ fontSize: '9px', color: FINCEPT.YELLOW, fontWeight: 700 }}>
                      MEMO/TAG REQUIRED
                    </span>
                  </div>
                  <div
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px',
                    }}
                  >
                    <div
                      style={{
                        flex: 1,
                        padding: '12px',
                        backgroundColor: FINCEPT.PANEL_BG,
                        borderRadius: '4px',
                        fontFamily: '"IBM Plex Mono", monospace',
                        fontSize: '11px',
                        color: FINCEPT.YELLOW,
                      }}
                    >
                      {address.tag}
                    </div>
                    <button
                      onClick={() => copyToClipboard(address.tag!, 'tag')}
                      style={{
                        padding: '12px',
                        backgroundColor: copied === 'tag' ? FINCEPT.GREEN : FINCEPT.YELLOW,
                        border: 'none',
                        borderRadius: '4px',
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                      }}
                      title="Copy memo/tag"
                    >
                      {copied === 'tag' ? (
                        <Check size={16} color={FINCEPT.DARK_BG} />
                      ) : (
                        <Copy size={16} color={FINCEPT.DARK_BG} />
                      )}
                    </button>
                  </div>
                  <div style={{ fontSize: '9px', color: FINCEPT.YELLOW, marginTop: '8px' }}>
                    Always include this memo/tag when depositing or your funds may be lost!
                  </div>
                </div>
              )}

              {/* Warning */}
              <div
                style={{
                  backgroundColor: `${FINCEPT.ORANGE}10`,
                  borderRadius: '4px',
                  padding: '12px',
                  fontSize: '10px',
                  color: FINCEPT.ORANGE,
                }}
              >
                <strong>Important:</strong> Only send {selectedCurrency}
                {address.network ? ` on the ${address.network} network` : ''} to this address. Sending
                other assets may result in permanent loss.
              </div>
            </div>
          )}
        </div>
      )}

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}
