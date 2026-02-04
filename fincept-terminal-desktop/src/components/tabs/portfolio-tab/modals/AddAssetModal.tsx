import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { marketDataService } from '../../../../services/markets/marketDataService';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, createFocusHandlers } from '../finceptStyles';

interface AddAssetModalProps {
  show: boolean;
  formState: {
    symbol: string;
    quantity: string;
    price: string;
  };
  onSymbolChange: (value: string) => void;
  onQuantityChange: (value: string) => void;
  onPriceChange: (value: string) => void;
  onClose: () => void;
  onAdd: () => void;
}

const AddAssetModal: React.FC<AddAssetModalProps> = ({
  show,
  formState,
  onSymbolChange,
  onQuantityChange,
  onPriceChange,
  onClose,
  onAdd
}) => {
  const [fetchingPrice, setFetchingPrice] = useState(false);
  const [priceError, setPriceError] = useState<string | null>(null);
  const [resolvedExchange, setResolvedExchange] = useState<string>('');

  // Auto-resolve symbol and fetch price when symbol changes
  useEffect(() => {
    const resolveAndFetch = async () => {
      const rawSymbol = formState.symbol.trim().toUpperCase();

      if (!rawSymbol || rawSymbol.length < 2) {
        setResolvedExchange('');
        return;
      }

      setFetchingPrice(true);
      setPriceError(null);
      setResolvedExchange('');

      try {
        // Step 1: Resolve the symbol to its correct yfinance form
        // e.g. "PIDILITIND" → "PIDILITIND.NS", "AAPL" → "AAPL"
        let symbolToUse = rawSymbol;
        try {
          const resolveOutput = await invoke<string>('execute_yfinance_command', {
            command: 'resolve_symbol',
            args: [rawSymbol],
          });
          const resolved = JSON.parse(resolveOutput);
          if (resolved.found && resolved.resolved_symbol) {
            symbolToUse = resolved.resolved_symbol;
            // Update the symbol field to the resolved form so it gets stored correctly
            if (symbolToUse !== rawSymbol) {
              onSymbolChange(symbolToUse);
            }
            if (resolved.exchange) {
              setResolvedExchange(resolved.exchange);
            }
          }
        } catch (resolveErr) {
          console.warn('[AddAsset] Symbol resolution failed, using as-is:', resolveErr);
        }

        // Step 2: Fetch the quote using the resolved symbol
        const quote = await marketDataService.getQuote(symbolToUse);

        if (quote && quote.price) {
          onPriceChange(quote.price.toFixed(2));
          setPriceError(null);
        } else {
          setPriceError('Price not found');
        }
      } catch (error) {
        console.error('Failed to fetch price:', error);
        setPriceError('Failed to fetch');
      } finally {
        setFetchingPrice(false);
      }
    };

    // Debounce: wait 800ms after user stops typing
    const timer = setTimeout(resolveAndFetch, 800);
    return () => clearTimeout(timer);
  }, [formState.symbol]);

  if (!show) return null;

  return (
    <div style={COMMON_STYLES.modalOverlay}>
      <div style={{
        ...COMMON_STYLES.modalPanel,
        border: BORDERS.GREEN,
        borderWidth: '2px',
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <div style={{
          color: FINCEPT.GREEN,
          fontSize: TYPOGRAPHY.HEADING,
          fontWeight: TYPOGRAPHY.BOLD,
          marginBottom: SPACING.LARGE,
          letterSpacing: TYPOGRAPHY.WIDE
        }}>
          ADD ASSET TO PORTFOLIO
        </div>

        <div style={{ marginBottom: SPACING.DEFAULT }}>
          <label style={{
            ...COMMON_STYLES.dataLabel,
            display: 'block',
            marginBottom: SPACING.SMALL
          }}>
            SYMBOL *
          </label>
          <input
            type="text"
            value={formState.symbol}
            onChange={(e) => onSymbolChange(e.target.value.toUpperCase())}
            style={{
              ...COMMON_STYLES.inputField,
              textTransform: 'uppercase'
            }}
            {...createFocusHandlers()}
            placeholder="AAPL"
          />
        </div>

        <div style={{ marginBottom: SPACING.DEFAULT }}>
          <label style={{
            ...COMMON_STYLES.dataLabel,
            display: 'block',
            marginBottom: SPACING.SMALL
          }}>
            QUANTITY *
          </label>
          <input
            type="text"
            inputMode="decimal"
            value={formState.quantity}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) onQuantityChange(v); }}
            style={COMMON_STYLES.inputField}
            {...createFocusHandlers()}
            placeholder="100"
          />
        </div>

        <div style={{ marginBottom: SPACING.LARGE }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: SPACING.SMALL
          }}>
            <label style={COMMON_STYLES.dataLabel}>
              BUY PRICE *
            </label>
            {fetchingPrice && (
              <span style={{ color: FINCEPT.YELLOW, fontSize: TYPOGRAPHY.TINY }}>
                ● Fetching...
              </span>
            )}
            {!fetchingPrice && priceError && (
              <span style={{ color: FINCEPT.RED, fontSize: TYPOGRAPHY.TINY }}>
                {priceError}
              </span>
            )}
            {!fetchingPrice && !priceError && formState.price && formState.symbol && (
              <span style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.TINY }}>
                {resolvedExchange ? `✓ ${resolvedExchange}` : '✓ Auto-fetched'}
              </span>
            )}
          </div>
          <input
            type="text"
            inputMode="decimal"
            value={formState.price}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) onPriceChange(v); }}
            style={{
              ...COMMON_STYLES.inputField,
              borderColor: fetchingPrice ? FINCEPT.YELLOW : FINCEPT.BORDER
            }}
            {...createFocusHandlers()}
            placeholder="150.00"
          />
        </div>

        <div style={{ display: 'flex', gap: SPACING.MEDIUM, justifyContent: 'flex-end' }}>
          <button
            onClick={onClose}
            style={COMMON_STYLES.buttonSecondary}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
            }}
          >
            CANCEL
          </button>
          <button
            onClick={onAdd}
            style={{
              ...COMMON_STYLES.buttonPrimary,
              backgroundColor: FINCEPT.GREEN,
              borderColor: FINCEPT.GREEN
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}
          >
            ADD
          </button>
        </div>
      </div>
    </div>
  );
};

export default AddAssetModal;
