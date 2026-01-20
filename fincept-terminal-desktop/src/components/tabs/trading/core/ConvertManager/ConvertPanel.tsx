/**
 * Convert Panel
 * Allows users to swap/convert between cryptocurrencies
 */

import React, { useState, useEffect } from 'react';
import { ArrowRightLeft, RefreshCw, AlertCircle, Check, Loader2, ArrowDownUp } from 'lucide-react';
import { useConvertCurrencies, useConvertQuote, useCreateConvertTrade } from '../../hooks/useConversions';
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

export function ConvertPanel() {
  const { tradingMode, activeBroker } = useBrokerContext();
  const { balances } = useBalance(false);
  const { currencies, isLoading: currenciesLoading, isSupported } = useConvertCurrencies();
  const { createTrade, isLoading: tradeLoading, error: tradeError, lastTrade } = useCreateConvertTrade();

  const [fromCurrency, setFromCurrency] = useState('');
  const [toCurrency, setToCurrency] = useState('');
  const [amount, setAmount] = useState('');
  const [showConfirm, setShowConfirm] = useState(false);
  const [success, setSuccess] = useState(false);

  // Get quote when currencies and amount are set
  const numericAmount = parseFloat(amount) || 0;
  const { quote, isLoading: quoteLoading, refresh: refreshQuote } = useConvertQuote(
    fromCurrency || undefined,
    toCurrency || undefined,
    numericAmount > 0 ? numericAmount : undefined
  );

  // Available currencies with balance
  const availableCurrencies = balances.filter((b) => b.free > 0).map((b) => b.currency);
  const fromBalance = balances.find((b) => b.currency === fromCurrency)?.free || 0;

  // Swap currencies
  const handleSwap = () => {
    const temp = fromCurrency;
    setFromCurrency(toCurrency);
    setToCurrency(temp);
    setAmount('');
  };

  // Handle amount change
  const handleAmountChange = (value: string) => {
    if (/^\d*\.?\d*$/.test(value)) {
      setAmount(value);
    }
  };

  // Set max amount
  const handleSetMax = () => {
    setAmount(fromBalance.toString());
  };

  // Execute conversion
  const handleConvert = async () => {
    if (!fromCurrency || !toCurrency || numericAmount <= 0) return;

    const result = await createTrade(fromCurrency, toCurrency, numericAmount, quote?.id);

    if (result) {
      setSuccess(true);
      setTimeout(() => {
        setAmount('');
        setShowConfirm(false);
        setSuccess(false);
      }, 3000);
    }
  };

  // Reset on currency change
  useEffect(() => {
    setShowConfirm(false);
    setSuccess(false);
  }, [fromCurrency, toCurrency]);

  if (tradingMode === 'paper') {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <AlertCircle size={32} color={FINCEPT.YELLOW} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px', marginBottom: '8px' }}>
          Currency conversion not available in paper trading mode
        </div>
      </div>
    );
  }

  if (!isSupported) {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <AlertCircle size={32} color={FINCEPT.GRAY} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px', marginBottom: '8px' }}>
          Currency conversion not supported by {activeBroker}
        </div>
      </div>
    );
  }

  return (
    <div style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '16px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '16px', paddingBottom: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <ArrowRightLeft size={14} color={FINCEPT.CYAN} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>Convert Crypto</span>
        </div>
      </div>

      {/* Success Message */}
      {success && lastTrade && (
        <div style={{ padding: '16px', backgroundColor: `${FINCEPT.GREEN}20`, borderRadius: '4px', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Check size={20} color={FINCEPT.GREEN} />
          <div>
            <div style={{ fontSize: '12px', color: FINCEPT.GREEN, fontWeight: 600 }}>Conversion Successful!</div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
              Converted {lastTrade.fromAmount} {lastTrade.fromCurrency} to {lastTrade.toAmount.toFixed(8)} {lastTrade.toCurrency}
            </div>
          </div>
        </div>
      )}

      {/* Error Message */}
      {tradeError && !success && (
        <div style={{ padding: '12px', backgroundColor: `${FINCEPT.RED}20`, borderRadius: '4px', marginBottom: '16px', fontSize: '11px', color: FINCEPT.RED }}>
          {tradeError.message}
        </div>
      )}

      {/* From Currency */}
      <div style={{ marginBottom: '8px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
          <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>FROM</label>
          <span style={{ fontSize: '9px', color: FINCEPT.CYAN }}>
            Balance: {fromBalance.toFixed(8)} {fromCurrency || '--'}
          </span>
        </div>
        <div style={{ display: 'flex', gap: '8px' }}>
          <select
            value={fromCurrency}
            onChange={(e) => setFromCurrency(e.target.value)}
            style={{
              width: '120px',
              padding: '10px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '4px',
              color: FINCEPT.WHITE,
              fontSize: '12px',
            }}
          >
            <option value="">Select</option>
            {availableCurrencies.map((c) => (
              <option key={c} value={c} disabled={c === toCurrency}>{c}</option>
            ))}
          </select>
          <div style={{ flex: 1, display: 'flex', gap: '4px' }}>
            <input
              type="text"
              value={amount}
              onChange={(e) => handleAmountChange(e.target.value)}
              placeholder="0.00"
              style={{
                flex: 1,
                padding: '10px',
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
              disabled={!fromCurrency}
              style={{
                padding: '10px 12px',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '4px',
                color: FINCEPT.ORANGE,
                fontSize: '9px',
                fontWeight: 700,
                cursor: fromCurrency ? 'pointer' : 'not-allowed',
              }}
            >
              MAX
            </button>
          </div>
        </div>
      </div>

      {/* Swap Button */}
      <div style={{ display: 'flex', justifyContent: 'center', padding: '8px 0' }}>
        <button
          onClick={handleSwap}
          disabled={!fromCurrency && !toCurrency}
          style={{
            padding: '8px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '50%',
            cursor: 'pointer',
          }}
        >
          <ArrowDownUp size={16} color={FINCEPT.ORANGE} />
        </button>
      </div>

      {/* To Currency */}
      <div style={{ marginBottom: '16px' }}>
        <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px' }}>TO</label>
        <div style={{ display: 'flex', gap: '8px' }}>
          <select
            value={toCurrency}
            onChange={(e) => setToCurrency(e.target.value)}
            style={{
              width: '120px',
              padding: '10px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '4px',
              color: FINCEPT.WHITE,
              fontSize: '12px',
            }}
          >
            <option value="">Select</option>
            {(currencies.length > 0 ? currencies : ['BTC', 'ETH', 'USDT', 'USDC', 'BNB']).map((c) => (
              <option key={c} value={c} disabled={c === fromCurrency}>{c}</option>
            ))}
          </select>
          <div
            style={{
              flex: 1,
              padding: '10px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '4px',
              color: FINCEPT.GREEN,
              fontSize: '14px',
              fontFamily: '"IBM Plex Mono", monospace',
              display: 'flex',
              alignItems: 'center',
            }}
          >
            {quoteLoading ? (
              <Loader2 size={14} color={FINCEPT.GRAY} style={{ animation: 'spin 1s linear infinite' }} />
            ) : quote ? (
              `~${quote.toAmount.toFixed(8)}`
            ) : (
              '--'
            )}
          </div>
        </div>
      </div>

      {/* Rate Display */}
      {quote && (
        <div style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px', marginBottom: '16px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
            <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Rate</span>
            <span style={{ fontSize: '10px', color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace' }}>
              1 {fromCurrency} = {quote.rate.toFixed(8)} {toCurrency}
            </span>
          </div>
          {quote.fee !== undefined && (
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Fee</span>
              <span style={{ fontSize: '10px', color: FINCEPT.ORANGE, fontFamily: '"IBM Plex Mono", monospace' }}>
                {quote.fee} {quote.feeCurrency || fromCurrency}
              </span>
            </div>
          )}
          <button
            onClick={refreshQuote}
            disabled={quoteLoading}
            style={{
              marginTop: '8px',
              width: '100%',
              padding: '6px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '4px',
              color: FINCEPT.GRAY,
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
            }}
          >
            <RefreshCw size={10} style={{ animation: quoteLoading ? 'spin 1s linear infinite' : 'none' }} />
            Refresh Quote
          </button>
        </div>
      )}

      {/* Convert Button */}
      {!showConfirm ? (
        <button
          onClick={() => setShowConfirm(true)}
          disabled={!fromCurrency || !toCurrency || numericAmount <= 0 || numericAmount > fromBalance || !quote}
          style={{
            width: '100%',
            padding: '14px',
            backgroundColor: (fromCurrency && toCurrency && numericAmount > 0 && numericAmount <= fromBalance && quote) ? FINCEPT.ORANGE : FINCEPT.BORDER,
            border: 'none',
            borderRadius: '4px',
            color: (fromCurrency && toCurrency && numericAmount > 0 && numericAmount <= fromBalance && quote) ? FINCEPT.WHITE : FINCEPT.GRAY,
            fontSize: '12px',
            fontWeight: 700,
            cursor: (fromCurrency && toCurrency && numericAmount > 0 && numericAmount <= fromBalance && quote) ? 'pointer' : 'not-allowed',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '8px',
          }}
        >
          <ArrowRightLeft size={14} />
          PREVIEW CONVERSION
        </button>
      ) : (
        <div>
          <div style={{ padding: '12px', backgroundColor: `${FINCEPT.CYAN}15`, border: `1px solid ${FINCEPT.CYAN}`, borderRadius: '4px', marginBottom: '12px', fontSize: '10px' }}>
            <div style={{ marginBottom: '4px', color: FINCEPT.CYAN, fontWeight: 600 }}>Confirm Conversion</div>
            <div style={{ color: FINCEPT.WHITE }}>
              Convert <strong>{amount} {fromCurrency}</strong> to approximately <strong>{quote?.toAmount.toFixed(8)} {toCurrency}</strong>
            </div>
          </div>
          <div style={{ display: 'flex', gap: '8px' }}>
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
              onClick={handleConvert}
              disabled={tradeLoading}
              style={{
                flex: 2,
                padding: '12px',
                backgroundColor: FINCEPT.GREEN,
                border: 'none',
                borderRadius: '4px',
                color: FINCEPT.WHITE,
                fontSize: '11px',
                fontWeight: 700,
                cursor: tradeLoading ? 'wait' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px',
              }}
            >
              {tradeLoading ? (
                <>
                  <Loader2 size={14} style={{ animation: 'spin 1s linear infinite' }} />
                  CONVERTING...
                </>
              ) : (
                <>
                  <Check size={14} />
                  CONFIRM CONVERT
                </>
              )}
            </button>
          </div>
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
