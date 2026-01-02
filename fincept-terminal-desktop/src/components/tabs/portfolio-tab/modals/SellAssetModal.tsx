import React, { useState, useEffect } from 'react';
import { marketDataService } from '../../../../services/marketDataService';
import { BLOOMBERG, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, createFocusHandlers } from '../bloombergStyles';

interface SellAssetModalProps {
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
  onSell: () => void;
}

const SellAssetModal: React.FC<SellAssetModalProps> = ({
  show,
  formState,
  onSymbolChange,
  onQuantityChange,
  onPriceChange,
  onClose,
  onSell
}) => {
  const [fetchingPrice, setFetchingPrice] = useState(false);
  const [priceError, setPriceError] = useState<string | null>(null);

  // Auto-fetch price when symbol changes
  useEffect(() => {
    const fetchPrice = async () => {
      const symbol = formState.symbol.trim().toUpperCase();

      if (!symbol || symbol.length < 2) {
        return;
      }

      setFetchingPrice(true);
      setPriceError(null);

      try {
        const quote = await marketDataService.getQuote(symbol);

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
    const timer = setTimeout(fetchPrice, 800);
    return () => clearTimeout(timer);
  }, [formState.symbol]);

  if (!show) return null;

  return (
    <div style={COMMON_STYLES.modalOverlay}>
      <div style={{
        ...COMMON_STYLES.modalPanel,
        border: BORDERS.RED,
        borderWidth: '2px',
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <div style={{
          color: BLOOMBERG.RED,
          fontSize: TYPOGRAPHY.HEADING,
          fontWeight: TYPOGRAPHY.BOLD,
          marginBottom: SPACING.LARGE,
          letterSpacing: TYPOGRAPHY.WIDE
        }}>
          SELL ASSET FROM PORTFOLIO
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
            type="number"
            value={formState.quantity}
            onChange={(e) => onQuantityChange(e.target.value)}
            style={COMMON_STYLES.inputField}
            {...createFocusHandlers()}
            placeholder="50"
            step="0.0001"
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
              SELL PRICE *
            </label>
            {fetchingPrice && (
              <span style={{ color: BLOOMBERG.YELLOW, fontSize: TYPOGRAPHY.TINY }}>
                ● Fetching...
              </span>
            )}
            {!fetchingPrice && priceError && (
              <span style={{ color: BLOOMBERG.RED, fontSize: TYPOGRAPHY.TINY }}>
                {priceError}
              </span>
            )}
            {!fetchingPrice && !priceError && formState.price && formState.symbol && (
              <span style={{ color: BLOOMBERG.CYAN, fontSize: TYPOGRAPHY.TINY }}>
                ✓ Auto-fetched
              </span>
            )}
          </div>
          <input
            type="number"
            value={formState.price}
            onChange={(e) => onPriceChange(e.target.value)}
            style={{
              ...COMMON_STYLES.inputField,
              borderColor: fetchingPrice ? BLOOMBERG.YELLOW : BLOOMBERG.BORDER
            }}
            {...createFocusHandlers()}
            placeholder="180.00"
            step="0.01"
          />
        </div>

        <div style={{ display: 'flex', gap: SPACING.MEDIUM, justifyContent: 'flex-end' }}>
          <button
            onClick={onClose}
            style={COMMON_STYLES.buttonSecondary}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
            }}
          >
            CANCEL
          </button>
          <button
            onClick={onSell}
            style={{
              ...COMMON_STYLES.buttonPrimary,
              backgroundColor: BLOOMBERG.RED,
              borderColor: BLOOMBERG.RED,
              color: BLOOMBERG.WHITE
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}
          >
            SELL
          </button>
        </div>
      </div>
    </div>
  );
};

export default SellAssetModal;
