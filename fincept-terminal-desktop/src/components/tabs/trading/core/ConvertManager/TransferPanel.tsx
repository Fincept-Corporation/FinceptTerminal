/**
 * Transfer Panel
 * Allows users to transfer funds between accounts (spot, margin, futures, etc.)
 */

import React, { useState } from 'react';
import { ArrowRight, RefreshCw, AlertCircle, Check, Loader2 } from 'lucide-react';
import { useTransfer, useAvailableAccounts } from '../../hooks/useConversions';
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
  PURPLE: '#9D4EDD',
};

export function TransferPanel() {
  const { tradingMode, activeBroker } = useBrokerContext();
  const { balances } = useBalance(false);
  const accounts = useAvailableAccounts();
  const { transfer, isLoading, error, lastTransfer, isSupported } = useTransfer();

  const [currency, setCurrency] = useState('');
  const [amount, setAmount] = useState('');
  const [fromAccount, setFromAccount] = useState('');
  const [toAccount, setToAccount] = useState('');
  const [success, setSuccess] = useState(false);

  // Get balance for selected currency
  const selectedBalance = balances.find((b) => b.currency === currency)?.free || 0;

  // Handle amount change
  const handleAmountChange = (value: string) => {
    if (/^\d*\.?\d*$/.test(value)) {
      setAmount(value);
    }
  };

  // Set max amount
  const handleSetMax = () => {
    setAmount(selectedBalance.toString());
  };

  // Swap accounts
  const handleSwapAccounts = () => {
    const temp = fromAccount;
    setFromAccount(toAccount);
    setToAccount(temp);
  };

  // Execute transfer
  const handleTransfer = async () => {
    const numericAmount = parseFloat(amount);
    if (!currency || numericAmount <= 0 || !fromAccount || !toAccount) return;

    const result = await transfer(currency, numericAmount, fromAccount, toAccount);

    if (result) {
      setSuccess(true);
      setTimeout(() => {
        setAmount('');
        setSuccess(false);
      }, 3000);
    }
  };

  const isValidForm = () => {
    const numericAmount = parseFloat(amount) || 0;
    return (
      currency &&
      numericAmount > 0 &&
      numericAmount <= selectedBalance &&
      fromAccount &&
      toAccount &&
      fromAccount !== toAccount
    );
  };

  if (tradingMode === 'paper') {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <AlertCircle size={32} color={FINCEPT.YELLOW} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px', marginBottom: '8px' }}>
          Internal transfers not available in paper trading mode
        </div>
      </div>
    );
  }

  if (!isSupported) {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <AlertCircle size={32} color={FINCEPT.GRAY} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px', marginBottom: '8px' }}>
          Internal transfers not supported by {activeBroker}
        </div>
      </div>
    );
  }

  return (
    <div style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '16px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '16px', paddingBottom: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <ArrowRight size={14} color={FINCEPT.PURPLE} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>Internal Transfer</span>
        </div>
      </div>

      {/* Success Message */}
      {success && lastTransfer && (
        <div style={{ padding: '16px', backgroundColor: `${FINCEPT.GREEN}20`, borderRadius: '4px', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Check size={20} color={FINCEPT.GREEN} />
          <div>
            <div style={{ fontSize: '12px', color: FINCEPT.GREEN, fontWeight: 600 }}>Transfer Successful!</div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
              Moved {lastTransfer.amount} {lastTransfer.currency} from {lastTransfer.fromAccount} to {lastTransfer.toAccount}
            </div>
          </div>
        </div>
      )}

      {/* Error Message */}
      {error && !success && (
        <div style={{ padding: '12px', backgroundColor: `${FINCEPT.RED}20`, borderRadius: '4px', marginBottom: '16px', fontSize: '11px', color: FINCEPT.RED }}>
          {error.message}
        </div>
      )}

      {/* Currency Selection */}
      <div style={{ marginBottom: '16px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>CURRENCY</label>
          <span style={{ fontSize: '9px', color: FINCEPT.CYAN }}>
            Available: {selectedBalance.toFixed(8)} {currency || '--'}
          </span>
        </div>
        <select
          value={currency}
          onChange={(e) => setCurrency(e.target.value)}
          style={{
            width: '100%',
            padding: '10px 12px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px',
            color: FINCEPT.WHITE,
            fontSize: '12px',
          }}
        >
          <option value="">Select currency</option>
          {balances.filter((b) => b.free > 0).map((b) => (
            <option key={b.currency} value={b.currency}>
              {b.currency} ({b.free.toFixed(8)} available)
            </option>
          ))}
        </select>
      </div>

      {/* Amount Input */}
      <div style={{ marginBottom: '16px' }}>
        <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px' }}>AMOUNT</label>
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
            disabled={!currency}
            style={{
              padding: '10px 16px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '4px',
              color: FINCEPT.ORANGE,
              fontSize: '10px',
              fontWeight: 700,
              cursor: currency ? 'pointer' : 'not-allowed',
            }}
          >
            MAX
          </button>
        </div>
      </div>

      {/* Account Selection */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr auto 1fr', gap: '8px', alignItems: 'end', marginBottom: '16px' }}>
        {/* From Account */}
        <div>
          <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px' }}>FROM</label>
          <select
            value={fromAccount}
            onChange={(e) => setFromAccount(e.target.value)}
            style={{
              width: '100%',
              padding: '10px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '4px',
              color: FINCEPT.WHITE,
              fontSize: '11px',
            }}
          >
            <option value="">Select</option>
            {accounts.map((acc) => (
              <option key={acc.id} value={acc.id} disabled={acc.id === toAccount}>
                {acc.name}
              </option>
            ))}
          </select>
        </div>

        {/* Swap Button */}
        <button
          onClick={handleSwapAccounts}
          disabled={!fromAccount && !toAccount}
          style={{
            padding: '10px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px',
            cursor: 'pointer',
            marginBottom: '0px',
          }}
        >
          <ArrowRight size={14} color={FINCEPT.PURPLE} />
        </button>

        {/* To Account */}
        <div>
          <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px' }}>TO</label>
          <select
            value={toAccount}
            onChange={(e) => setToAccount(e.target.value)}
            style={{
              width: '100%',
              padding: '10px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '4px',
              color: FINCEPT.WHITE,
              fontSize: '11px',
            }}
          >
            <option value="">Select</option>
            {accounts.map((acc) => (
              <option key={acc.id} value={acc.id} disabled={acc.id === fromAccount}>
                {acc.name}
              </option>
            ))}
          </select>
        </div>
      </div>

      {/* Account Descriptions */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '16px' }}>
        {fromAccount && (
          <div style={{ padding: '8px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px', fontSize: '9px', color: FINCEPT.GRAY }}>
            <strong style={{ color: FINCEPT.WHITE }}>From:</strong> {accounts.find((a) => a.id === fromAccount)?.description}
          </div>
        )}
        {toAccount && (
          <div style={{ padding: '8px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px', fontSize: '9px', color: FINCEPT.GRAY }}>
            <strong style={{ color: FINCEPT.WHITE }}>To:</strong> {accounts.find((a) => a.id === toAccount)?.description}
          </div>
        )}
      </div>

      {/* Transfer Button */}
      <button
        onClick={handleTransfer}
        disabled={!isValidForm() || isLoading}
        style={{
          width: '100%',
          padding: '14px',
          backgroundColor: isValidForm() && !isLoading ? FINCEPT.PURPLE : FINCEPT.BORDER,
          border: 'none',
          borderRadius: '4px',
          color: isValidForm() && !isLoading ? FINCEPT.WHITE : FINCEPT.GRAY,
          fontSize: '12px',
          fontWeight: 700,
          cursor: isValidForm() && !isLoading ? 'pointer' : 'not-allowed',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '8px',
        }}
      >
        {isLoading ? (
          <>
            <Loader2 size={14} style={{ animation: 'spin 1s linear infinite' }} />
            TRANSFERRING...
          </>
        ) : (
          <>
            <ArrowRight size={14} />
            TRANSFER FUNDS
          </>
        )}
      </button>

      {/* Info */}
      <div style={{ marginTop: '12px', padding: '10px', backgroundColor: `${FINCEPT.PURPLE}15`, borderRadius: '4px', fontSize: '9px', color: FINCEPT.GRAY }}>
        <strong style={{ color: FINCEPT.PURPLE }}>Note:</strong> Internal transfers are instant and free.
        Move funds between your accounts to enable different trading features.
      </div>

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}
