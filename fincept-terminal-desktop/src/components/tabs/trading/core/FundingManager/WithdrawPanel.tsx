/**
 * Withdraw Panel
 * Form for initiating crypto withdrawals
 */

import React, { useState, useEffect } from 'react';
import { Send, AlertCircle, AlertTriangle, Check, Loader2 } from 'lucide-react';
import { useWithdraw } from '../../hooks/useFunding';
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

// Common networks for withdrawals
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

export function WithdrawPanel() {
  const { tradingMode, activeBroker } = useBrokerContext();
  const { balances } = useBalance(false);
  const { withdraw, isLoading, error, lastResult, isSupported } = useWithdraw();

  const [selectedCurrency, setSelectedCurrency] = useState<string>('');
  const [selectedNetwork, setSelectedNetwork] = useState<string>('');
  const [amount, setAmount] = useState<string>('');
  const [address, setAddress] = useState<string>('');
  const [tag, setTag] = useState<string>('');
  const [showConfirm, setShowConfirm] = useState(false);
  const [success, setSuccess] = useState(false);

  // Get balance for selected currency
  const selectedBalance = balances.find((b) => b.currency === selectedCurrency);
  const availableBalance = selectedBalance?.free || 0;

  // Reset form on currency change
  useEffect(() => {
    setSelectedNetwork('');
    setAmount('');
    setAddress('');
    setTag('');
    setShowConfirm(false);
    setSuccess(false);
  }, [selectedCurrency]);

  // Check if tag/memo is required
  const requiresTag = ['XRP', 'XLM', 'EOS', 'ATOM', 'BNB'].includes(selectedCurrency);

  const handleAmountChange = (value: string) => {
    // Only allow numbers and decimal
    if (/^\d*\.?\d*$/.test(value)) {
      setAmount(value);
    }
  };

  const handleSetMax = () => {
    setAmount(availableBalance.toString());
  };

  const handleSetPercent = (percent: number) => {
    setAmount((availableBalance * percent).toFixed(8));
  };

  const isValidForm = () => {
    if (!selectedCurrency) return false;
    if (!amount || parseFloat(amount) <= 0) return false;
    if (parseFloat(amount) > availableBalance) return false;
    if (!address || address.length < 10) return false;
    if (requiresTag && !tag) return false;
    return true;
  };

  const handleWithdraw = async () => {
    if (!isValidForm()) return;

    const result = await withdraw({
      code: selectedCurrency,
      amount: parseFloat(amount),
      address,
      tag: tag || undefined,
      network: selectedNetwork || undefined,
    });

    if (result) {
      setSuccess(true);
      // Reset form after successful withdrawal
      setTimeout(() => {
        setAmount('');
        setAddress('');
        setTag('');
        setShowConfirm(false);
        setSuccess(false);
      }, 3000);
    }
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
          Withdrawals not available in paper trading mode
        </div>
        <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
          Connect a live exchange to withdraw funds
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
          Withdraw from {activeBroker?.charAt(0).toUpperCase()}{activeBroker?.slice(1)}
        </div>
      </div>

      {/* Success Message */}
      {success && (
        <div
          style={{
            padding: '16px',
            backgroundColor: `${FINCEPT.GREEN}20`,
            borderRadius: '4px',
            marginBottom: '16px',
            display: 'flex',
            alignItems: 'center',
            gap: '12px',
          }}
        >
          <Check size={20} color={FINCEPT.GREEN} />
          <div>
            <div style={{ fontSize: '12px', color: FINCEPT.GREEN, fontWeight: 600 }}>
              Withdrawal Submitted
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
              Your withdrawal is being processed
            </div>
          </div>
        </div>
      )}

      {/* Error Message */}
      {error && !success && (
        <div
          style={{
            padding: '12px',
            backgroundColor: `${FINCEPT.RED}20`,
            borderRadius: '4px',
            marginBottom: '16px',
            fontSize: '11px',
            color: FINCEPT.RED,
          }}
        >
          {error.message}
        </div>
      )}

      {/* Currency Selection */}
      <div style={{ marginBottom: '16px' }}>
        <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px' }}>
          SELECT CURRENCY
        </label>
        <select
          value={selectedCurrency}
          onChange={(e) => setSelectedCurrency(e.target.value)}
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
          {balances
            .filter((b) => b.free > 0)
            .map((b) => (
              <option key={b.currency} value={b.currency}>
                {b.currency} (Available: {b.free.toFixed(8)})
              </option>
            ))}
        </select>
      </div>

      {selectedCurrency && (
        <>
          {/* Network Selection */}
          {NETWORKS[selectedCurrency] && (
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

          {/* Amount Input */}
          <div style={{ marginBottom: '16px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
              <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>AMOUNT</label>
              <span style={{ fontSize: '9px', color: FINCEPT.CYAN }}>
                Available: {availableBalance.toFixed(8)} {selectedCurrency}
              </span>
            </div>
            <div style={{ display: 'flex', gap: '8px' }}>
              <input
                type="text"
                value={amount}
                onChange={(e) => handleAmountChange(e.target.value)}
                placeholder="0.00000000"
                style={{
                  flex: 1,
                  padding: '10px 12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '4px',
                  color: FINCEPT.WHITE,
                  fontSize: '14px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              />
              <button
                onClick={handleSetMax}
                style={{
                  padding: '10px 16px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '4px',
                  color: FINCEPT.ORANGE,
                  fontSize: '10px',
                  fontWeight: 700,
                  cursor: 'pointer',
                }}
              >
                MAX
              </button>
            </div>
            <div style={{ display: 'flex', gap: '8px', marginTop: '8px' }}>
              {[0.25, 0.5, 0.75].map((percent) => (
                <button
                  key={percent}
                  onClick={() => handleSetPercent(percent)}
                  style={{
                    padding: '4px 8px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    color: FINCEPT.GRAY,
                    fontSize: '9px',
                    cursor: 'pointer',
                  }}
                >
                  {percent * 100}%
                </button>
              ))}
            </div>
          </div>

          {/* Address Input */}
          <div style={{ marginBottom: '16px' }}>
            <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px' }}>
              WITHDRAWAL ADDRESS
            </label>
            <input
              type="text"
              value={address}
              onChange={(e) => setAddress(e.target.value)}
              placeholder="Enter destination address"
              style={{
                width: '100%',
                padding: '10px 12px',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '4px',
                color: FINCEPT.WHITE,
                fontSize: '12px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            />
          </div>

          {/* Tag/Memo Input (if required) */}
          {requiresTag && (
            <div style={{ marginBottom: '16px' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px' }}>
                <AlertTriangle size={10} color={FINCEPT.YELLOW} />
                <label style={{ fontSize: '9px', color: FINCEPT.YELLOW }}>
                  MEMO/TAG (REQUIRED)
                </label>
              </div>
              <input
                type="text"
                value={tag}
                onChange={(e) => setTag(e.target.value)}
                placeholder="Enter memo/tag"
                style={{
                  width: '100%',
                  padding: '10px 12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.YELLOW}`,
                  borderRadius: '4px',
                  color: FINCEPT.WHITE,
                  fontSize: '12px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              />
            </div>
          )}

          {/* Confirmation Checkbox */}
          {!showConfirm ? (
            <button
              onClick={() => setShowConfirm(true)}
              disabled={!isValidForm()}
              style={{
                width: '100%',
                padding: '12px',
                backgroundColor: isValidForm() ? FINCEPT.ORANGE : FINCEPT.BORDER,
                border: 'none',
                borderRadius: '4px',
                color: isValidForm() ? FINCEPT.WHITE : FINCEPT.GRAY,
                fontSize: '12px',
                fontWeight: 700,
                cursor: isValidForm() ? 'pointer' : 'not-allowed',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
              }}
            >
              <Send size={14} />
              CONTINUE TO WITHDRAW
            </button>
          ) : (
            <div>
              {/* Confirmation Summary */}
              <div
                style={{
                  backgroundColor: `${FINCEPT.YELLOW}10`,
                  border: `1px solid ${FINCEPT.YELLOW}`,
                  borderRadius: '4px',
                  padding: '16px',
                  marginBottom: '16px',
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px' }}>
                  <AlertTriangle size={16} color={FINCEPT.YELLOW} />
                  <span style={{ fontSize: '11px', color: FINCEPT.YELLOW, fontWeight: 700 }}>
                    CONFIRM WITHDRAWAL
                  </span>
                </div>

                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
                  <strong>Amount:</strong>{' '}
                  <span style={{ color: FINCEPT.WHITE }}>
                    {amount} {selectedCurrency}
                  </span>
                </div>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
                  <strong>To:</strong>{' '}
                  <span style={{ color: FINCEPT.CYAN, wordBreak: 'break-all' }}>{address}</span>
                </div>
                {selectedNetwork && (
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
                    <strong>Network:</strong> <span style={{ color: FINCEPT.WHITE }}>{selectedNetwork}</span>
                  </div>
                )}
                {tag && (
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                    <strong>Memo/Tag:</strong> <span style={{ color: FINCEPT.YELLOW }}>{tag}</span>
                  </div>
                )}
              </div>

              <div style={{ display: 'flex', gap: '12px' }}>
                <button
                  onClick={() => setShowConfirm(false)}
                  style={{
                    flex: 1,
                    padding: '12px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '4px',
                    color: FINCEPT.GRAY,
                    fontSize: '11px',
                    cursor: 'pointer',
                  }}
                >
                  CANCEL
                </button>
                <button
                  onClick={handleWithdraw}
                  disabled={isLoading}
                  style={{
                    flex: 2,
                    padding: '12px',
                    backgroundColor: FINCEPT.RED,
                    border: 'none',
                    borderRadius: '4px',
                    color: FINCEPT.WHITE,
                    fontSize: '11px',
                    fontWeight: 700,
                    cursor: isLoading ? 'wait' : 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '8px',
                  }}
                >
                  {isLoading ? (
                    <>
                      <Loader2 size={14} style={{ animation: 'spin 1s linear infinite' }} />
                      PROCESSING...
                    </>
                  ) : (
                    <>
                      <Send size={14} />
                      CONFIRM WITHDRAWAL
                    </>
                  )}
                </button>
              </div>
            </div>
          )}

          {/* Warning */}
          <div
            style={{
              marginTop: '16px',
              padding: '12px',
              backgroundColor: `${FINCEPT.RED}10`,
              borderRadius: '4px',
              fontSize: '9px',
              color: FINCEPT.RED,
            }}
          >
            <strong>Warning:</strong> Cryptocurrency transactions are irreversible. Please verify the
            address and network carefully before confirming.
          </div>
        </>
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
