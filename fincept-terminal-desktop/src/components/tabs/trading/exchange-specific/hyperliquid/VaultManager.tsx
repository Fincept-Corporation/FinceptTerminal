/**
 * HyperLiquid Vault Manager
 * Manage vault trading and subaccounts
 */

import React, { useState, useEffect } from 'react';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  GRAY: '#787878',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  CYAN: '#00E5FF',
};

export function HyperLiquidVaultManager() {
  const { activeBroker, activeAdapter } = useBrokerContext();
  const [vaultAddress, setVaultAddress] = useState('');
  const [subAccountAddress, setSubAccountAddress] = useState('');
  const [vaultBalance, setVaultBalance] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(false);

  // Only show for HyperLiquid
  if (activeBroker !== 'hyperliquid') {
    return null;
  }

  const handleSetVault = async () => {
    if (!vaultAddress || !activeAdapter) return;

    setIsLoading(true);
    try {
      (activeAdapter as any).setVaultAddress(vaultAddress);
      const balance = await (activeAdapter as any).fetchVaultBalance(vaultAddress);
      setVaultBalance(balance);
      alert(`✓ Vault connected: ${vaultAddress.slice(0, 8)}...`);
    } catch (error) {
      alert(`✗ Vault connection failed: ${(error as Error).message}`);
    } finally {
      setIsLoading(false);
    }
  };

  const handleSetSubAccount = () => {
    if (!subAccountAddress || !activeAdapter) return;
    (activeAdapter as any).setSubAccountAddress(subAccountAddress);
    alert(`✓ Subaccount set: ${subAccountAddress.slice(0, 8)}...`);
  };

  return (
    <div
      style={{
        padding: '12px',
        backgroundColor: BLOOMBERG.PANEL_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`,
        marginTop: '12px',
      }}
    >
      {/* Header */}
      <div
        style={{
          fontSize: '10px',
          fontWeight: 700,
          color: BLOOMBERG.ORANGE,
          marginBottom: '12px',
          letterSpacing: '0.5px',
        }}
      >
        ⚡ HYPERLIQUID VAULT & SUBACCOUNTS
      </div>

      {/* Vault Address */}
      <div style={{ marginBottom: '12px' }}>
        <label
          style={{
            display: 'block',
            fontSize: '9px',
            fontWeight: 700,
            color: BLOOMBERG.GRAY,
            marginBottom: '4px',
          }}
        >
          VAULT ADDRESS
        </label>
        <div style={{ display: 'flex', gap: '4px' }}>
          <input
            type="text"
            value={vaultAddress}
            onChange={(e) => setVaultAddress(e.target.value)}
            placeholder="0x..."
            style={{
              flex: 1,
              padding: '6px 8px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.WHITE,
              fontSize: '10px',
              fontFamily: 'inherit',
            }}
          />
          <button
            onClick={handleSetVault}
            disabled={isLoading || !vaultAddress}
            style={{
              padding: '6px 12px',
              backgroundColor: BLOOMBERG.CYAN,
              border: 'none',
              color: BLOOMBERG.PANEL_BG,
              fontSize: '9px',
              fontWeight: 700,
              cursor: !vaultAddress || isLoading ? 'not-allowed' : 'pointer',
              opacity: !vaultAddress || isLoading ? 0.5 : 1,
            }}
          >
            {isLoading ? '...' : 'CONNECT'}
          </button>
        </div>
      </div>

      {/* Subaccount Address */}
      <div style={{ marginBottom: '12px' }}>
        <label
          style={{
            display: 'block',
            fontSize: '9px',
            fontWeight: 700,
            color: BLOOMBERG.GRAY,
            marginBottom: '4px',
          }}
        >
          SUBACCOUNT ADDRESS
        </label>
        <div style={{ display: 'flex', gap: '4px' }}>
          <input
            type="text"
            value={subAccountAddress}
            onChange={(e) => setSubAccountAddress(e.target.value)}
            placeholder="0x..."
            style={{
              flex: 1,
              padding: '6px 8px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.WHITE,
              fontSize: '10px',
              fontFamily: 'inherit',
            }}
          />
          <button
            onClick={handleSetSubAccount}
            disabled={!subAccountAddress}
            style={{
              padding: '6px 12px',
              backgroundColor: BLOOMBERG.GREEN,
              border: 'none',
              color: BLOOMBERG.PANEL_BG,
              fontSize: '9px',
              fontWeight: 700,
              cursor: !subAccountAddress ? 'not-allowed' : 'pointer',
              opacity: !subAccountAddress ? 0.5 : 1,
            }}
          >
            SET
          </button>
        </div>
      </div>

      {/* Vault Balance */}
      {vaultBalance && (
        <div
          style={{
            padding: '8px',
            backgroundColor: `${BLOOMBERG.CYAN}15`,
            border: `1px solid ${BLOOMBERG.CYAN}40`,
            fontSize: '9px',
            color: BLOOMBERG.CYAN,
          }}
        >
          ✓ Vault Balance: ${vaultBalance.total?.USD?.toFixed(2) || '0.00'}
        </div>
      )}

      {/* Info */}
      <div
        style={{
          marginTop: '12px',
          padding: '8px',
          backgroundColor: `${BLOOMBERG.ORANGE}10`,
          border: `1px solid ${BLOOMBERG.ORANGE}40`,
          fontSize: '8px',
          color: BLOOMBERG.GRAY,
          lineHeight: '1.4',
        }}
      >
        💡 HyperLiquid vaults allow you to trade from a vault address. Subaccounts enable multi-strategy trading.
      </div>
    </div>
  );
}
