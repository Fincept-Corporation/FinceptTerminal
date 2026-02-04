/**
 * Advanced Orders Panel
 * Batch orders, cancel all orders, cancel after timeout
 */

import React, { useState } from 'react';
import { Layers, XCircle, Clock, AlertTriangle, Check, Loader2, Plus, Trash2 } from 'lucide-react';
import { useCancelAllOrders, useCancelAllOrdersAfter, useCloseAllPositions } from '../../hooks';
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

interface AdvancedOrdersPanelProps {
  symbol?: string;
}

export function AdvancedOrdersPanel({ symbol }: AdvancedOrdersPanelProps) {
  const { tradingMode, activeBroker } = useBrokerContext();
  const { cancelAllOrders, isLoading: cancellingAll, isSupported: cancelAllSupported } = useCancelAllOrders();
  const { cancelAllOrdersAfter, isLoading: settingTimeout, scheduledTime, isSupported: timeoutSupported } = useCancelAllOrdersAfter();
  const { closeAllPositions, isLoading: closingAll, isSupported: closeAllSupported } = useCloseAllPositions();

  const [timeoutSeconds, setTimeoutSeconds] = useState(60);
  const [confirmCancel, setConfirmCancel] = useState(false);
  const [confirmClose, setConfirmClose] = useState(false);
  const [success, setSuccess] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  // Handle cancel all orders
  const handleCancelAll = async () => {
    if (!confirmCancel) {
      setConfirmCancel(true);
      return;
    }

    setError(null);
    const result = await cancelAllOrders(symbol);
    if (result) {
      setSuccess(`Cancelled ${result.length} orders`);
      setTimeout(() => setSuccess(null), 3000);
    }
    setConfirmCancel(false);
  };

  // Handle cancel after timeout
  const handleCancelAfter = async () => {
    setError(null);
    const result = await cancelAllOrdersAfter(timeoutSeconds);
    if (result) {
      setSuccess(`Orders will be cancelled at ${new Date(result.triggerTime).toLocaleTimeString()}`);
      setTimeout(() => setSuccess(null), 5000);
    }
  };

  // Handle close all positions
  const handleCloseAll = async () => {
    if (!confirmClose) {
      setConfirmClose(true);
      return;
    }

    setError(null);
    const result = await closeAllPositions(symbol ? { symbol } : undefined);
    if (result) {
      setSuccess(`Closed ${result.length} positions`);
      setTimeout(() => setSuccess(null), 3000);
    }
    setConfirmClose(false);
  };

  if (tradingMode === 'paper') {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <AlertTriangle size={32} color={FINCEPT.YELLOW} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px' }}>Advanced orders not available in paper trading</div>
      </div>
    );
  }

  const isLoading = cancellingAll || settingTimeout || closingAll;

  return (
    <div style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '16px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '16px', paddingBottom: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Layers size={14} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>Advanced Orders</span>
        </div>
        {symbol && (
          <span style={{ fontSize: '10px', color: FINCEPT.CYAN, fontFamily: '"IBM Plex Mono", monospace' }}>{symbol}</span>
        )}
      </div>

      {/* Success Message */}
      {success && (
        <div style={{ padding: '10px', backgroundColor: `${FINCEPT.GREEN}20`, borderRadius: '4px', marginBottom: '12px', display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Check size={14} color={FINCEPT.GREEN} />
          <span style={{ fontSize: '11px', color: FINCEPT.GREEN }}>{success}</span>
        </div>
      )}

      {/* Error Message */}
      {error && (
        <div style={{ padding: '10px', backgroundColor: `${FINCEPT.RED}20`, borderRadius: '4px', marginBottom: '12px', fontSize: '11px', color: FINCEPT.RED }}>
          {error}
        </div>
      )}

      {/* Cancel All Orders */}
      {cancelAllSupported && (
        <div style={{ marginBottom: '16px', padding: '12px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '10px' }}>
            <XCircle size={12} color={FINCEPT.RED} />
            <span style={{ fontSize: '10px', color: FINCEPT.WHITE, fontWeight: 600 }}>CANCEL ALL ORDERS</span>
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '10px' }}>
            {symbol ? `Cancel all open orders for ${symbol}` : 'Cancel all open orders across all symbols'}
          </div>
          <button
            onClick={handleCancelAll}
            disabled={isLoading}
            style={{
              width: '100%',
              padding: '10px',
              backgroundColor: confirmCancel ? FINCEPT.RED : `${FINCEPT.RED}30`,
              border: `1px solid ${FINCEPT.RED}`,
              borderRadius: '4px',
              color: FINCEPT.WHITE,
              fontSize: '11px',
              fontWeight: 700,
              cursor: isLoading ? 'wait' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
            }}
          >
            {cancellingAll ? (
              <>
                <Loader2 size={12} style={{ animation: 'spin 1s linear infinite' }} />
                CANCELLING...
              </>
            ) : confirmCancel ? (
              <>
                <AlertTriangle size={12} />
                CONFIRM CANCEL ALL
              </>
            ) : (
              <>
                <XCircle size={12} />
                CANCEL ALL ORDERS
              </>
            )}
          </button>
          {confirmCancel && (
            <button
              onClick={() => setConfirmCancel(false)}
              style={{
                width: '100%',
                marginTop: '6px',
                padding: '8px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '4px',
                color: FINCEPT.GRAY,
                fontSize: '10px',
                cursor: 'pointer',
              }}
            >
              NEVERMIND
            </button>
          )}
        </div>
      )}

      {/* Cancel Orders After Timeout */}
      {timeoutSupported && (
        <div style={{ marginBottom: '16px', padding: '12px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '10px' }}>
            <Clock size={12} color={FINCEPT.YELLOW} />
            <span style={{ fontSize: '10px', color: FINCEPT.WHITE, fontWeight: 600 }}>DEAD MAN'S SWITCH</span>
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '10px' }}>
            Automatically cancel all orders after a timeout
          </div>
          <div style={{ display: 'flex', gap: '8px', marginBottom: '10px' }}>
            <input
              type="text"
              inputMode="numeric"
              value={timeoutSeconds}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) setTimeoutSeconds(Math.min(parseInt(v) || 0, 3600)); }}
              style={{
                flex: 1,
                padding: '8px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '4px',
                color: FINCEPT.WHITE,
                fontSize: '12px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            />
            <span style={{ display: 'flex', alignItems: 'center', fontSize: '10px', color: FINCEPT.GRAY }}>seconds</span>
          </div>
          {/* Quick Select */}
          <div style={{ display: 'flex', gap: '4px', marginBottom: '10px' }}>
            {[30, 60, 300, 600].map((sec) => (
              <button
                key={sec}
                onClick={() => setTimeoutSeconds(sec)}
                style={{
                  flex: 1,
                  padding: '6px',
                  backgroundColor: timeoutSeconds === sec ? FINCEPT.YELLOW : 'transparent',
                  border: `1px solid ${timeoutSeconds === sec ? FINCEPT.YELLOW : FINCEPT.BORDER}`,
                  borderRadius: '4px',
                  color: timeoutSeconds === sec ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  fontSize: '9px',
                  fontWeight: 600,
                  cursor: 'pointer',
                }}
              >
                {sec < 60 ? `${sec}s` : `${sec / 60}m`}
              </button>
            ))}
          </div>
          <button
            onClick={handleCancelAfter}
            disabled={isLoading || timeoutSeconds < 10}
            style={{
              width: '100%',
              padding: '10px',
              backgroundColor: `${FINCEPT.YELLOW}30`,
              border: `1px solid ${FINCEPT.YELLOW}`,
              borderRadius: '4px',
              color: FINCEPT.WHITE,
              fontSize: '11px',
              fontWeight: 700,
              cursor: isLoading ? 'wait' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
            }}
          >
            {settingTimeout ? (
              <>
                <Loader2 size={12} style={{ animation: 'spin 1s linear infinite' }} />
                SETTING...
              </>
            ) : (
              <>
                <Clock size={12} />
                SET TIMEOUT
              </>
            )}
          </button>
          {scheduledTime && (
            <div style={{ marginTop: '8px', padding: '8px', backgroundColor: `${FINCEPT.YELLOW}15`, borderRadius: '4px', fontSize: '9px', color: FINCEPT.YELLOW }}>
              Orders will cancel at {new Date(scheduledTime).toLocaleTimeString()}
            </div>
          )}
        </div>
      )}

      {/* Close All Positions */}
      {closeAllSupported && (
        <div style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '10px' }}>
            <Trash2 size={12} color={FINCEPT.PURPLE} />
            <span style={{ fontSize: '10px', color: FINCEPT.WHITE, fontWeight: 600 }}>CLOSE ALL POSITIONS</span>
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '10px' }}>
            {symbol ? `Close all positions for ${symbol}` : 'Close all open positions across all symbols'}
          </div>
          <button
            onClick={handleCloseAll}
            disabled={isLoading}
            style={{
              width: '100%',
              padding: '10px',
              backgroundColor: confirmClose ? FINCEPT.PURPLE : `${FINCEPT.PURPLE}30`,
              border: `1px solid ${FINCEPT.PURPLE}`,
              borderRadius: '4px',
              color: FINCEPT.WHITE,
              fontSize: '11px',
              fontWeight: 700,
              cursor: isLoading ? 'wait' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
            }}
          >
            {closingAll ? (
              <>
                <Loader2 size={12} style={{ animation: 'spin 1s linear infinite' }} />
                CLOSING...
              </>
            ) : confirmClose ? (
              <>
                <AlertTriangle size={12} />
                CONFIRM CLOSE ALL
              </>
            ) : (
              <>
                <Trash2 size={12} />
                CLOSE ALL POSITIONS
              </>
            )}
          </button>
          {confirmClose && (
            <button
              onClick={() => setConfirmClose(false)}
              style={{
                width: '100%',
                marginTop: '6px',
                padding: '8px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '4px',
                color: FINCEPT.GRAY,
                fontSize: '10px',
                cursor: 'pointer',
              }}
            >
              NEVERMIND
            </button>
          )}
        </div>
      )}

      {/* No Features Available */}
      {!cancelAllSupported && !timeoutSupported && !closeAllSupported && (
        <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
          No advanced order features available for {activeBroker}
        </div>
      )}

      {/* Warning */}
      <div style={{ marginTop: '12px', padding: '10px', backgroundColor: `${FINCEPT.RED}10`, borderRadius: '4px', display: 'flex', gap: '8px', alignItems: 'flex-start' }}>
        <AlertTriangle size={14} color={FINCEPT.RED} style={{ flexShrink: 0, marginTop: '2px' }} />
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
          <strong style={{ color: FINCEPT.RED }}>Warning:</strong> These actions are irreversible.
          Cancelled orders and closed positions cannot be restored.
        </div>
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
