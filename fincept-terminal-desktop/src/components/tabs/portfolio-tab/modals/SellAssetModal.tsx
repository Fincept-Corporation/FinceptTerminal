import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { marketDataService } from '../../../../services/markets/marketDataService';

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
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
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
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0,0,0,0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 9999,
    }}>
      <div style={{
        backgroundColor: colors.panel,
        border: `2px solid ${colors.alert}`,
        padding: '24px',
        minWidth: '400px',
        maxWidth: '600px',
        boxShadow: `0 4px 16px ${colors.background}80`,
        fontFamily,
      }}>
        <div style={{
          color: colors.alert,
          fontSize: fontSize.heading,
          fontWeight: 700,
          marginBottom: '16px',
          letterSpacing: '0.5px',
        }}>
          {t('modals.sellAssetTitle')}
        </div>

        <div style={{ marginBottom: '12px' }}>
          <label style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            display: 'block',
            marginBottom: '4px',
          }}>
            {t('modals.symbol')} *
          </label>
          <input
            type="text"
            value={formState.symbol}
            onChange={(e) => onSymbolChange(e.target.value.toUpperCase())}
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: colors.background,
              color: colors.text,
              border: '1px solid var(--ft-color-border, #2A2A2A)',
              borderRadius: '2px',
              fontSize: fontSize.small,
              fontFamily,
              outline: 'none',
              textTransform: 'uppercase',
            }}
            onFocus={(e) => { e.currentTarget.style.borderColor = colors.alert; }}
            onBlur={(e) => { e.currentTarget.style.borderColor = 'var(--ft-color-border, #2A2A2A)'; }}
            placeholder="AAPL"
          />
        </div>

        <div style={{ marginBottom: '12px' }}>
          <label style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            display: 'block',
            marginBottom: '4px',
          }}>
            {t('modals.quantity')} *
          </label>
          <input
            type="text"
            inputMode="decimal"
            value={formState.quantity}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) onQuantityChange(v); }}
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: colors.background,
              color: colors.text,
              border: '1px solid var(--ft-color-border, #2A2A2A)',
              borderRadius: '2px',
              fontSize: fontSize.small,
              fontFamily,
              outline: 'none',
            }}
            onFocus={(e) => { e.currentTarget.style.borderColor = colors.alert; }}
            onBlur={(e) => { e.currentTarget.style.borderColor = 'var(--ft-color-border, #2A2A2A)'; }}
            placeholder="50"
          />
        </div>

        <div style={{ marginBottom: '16px' }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: '4px'
          }}>
            <label style={{
              color: colors.textMuted,
              fontSize: fontSize.tiny,
              fontWeight: 700,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
            }}>
              {t('modals.price')} *
            </label>
            {fetchingPrice && (
              <span style={{ color: 'var(--ft-color-warning, #FFD700)', fontSize: fontSize.tiny }}>
                ● Fetching...
              </span>
            )}
            {!fetchingPrice && priceError && (
              <span style={{ color: colors.alert, fontSize: fontSize.tiny }}>
                {priceError}
              </span>
            )}
            {!fetchingPrice && !priceError && formState.price && formState.symbol && (
              <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.tiny }}>
                ✓ Auto-fetched
              </span>
            )}
          </div>
          <input
            type="text"
            inputMode="decimal"
            value={formState.price}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) onPriceChange(v); }}
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: colors.background,
              color: colors.text,
              border: `1px solid ${fetchingPrice ? 'var(--ft-color-warning, #FFD700)' : 'var(--ft-color-border, #2A2A2A)'}`,
              borderRadius: '2px',
              fontSize: fontSize.small,
              fontFamily,
              outline: 'none',
            }}
            onFocus={(e) => { e.currentTarget.style.borderColor = colors.alert; }}
            onBlur={(e) => { e.currentTarget.style.borderColor = fetchingPrice ? 'var(--ft-color-warning, #FFD700)' : 'var(--ft-color-border, #2A2A2A)'; }}
            placeholder="180.00"
          />
        </div>

        <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
          <button
            onClick={onClose}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: '1px solid var(--ft-color-border, #2A2A2A)',
              color: colors.textMuted,
              borderRadius: '2px',
              fontSize: fontSize.tiny,
              fontWeight: 700,
              cursor: 'pointer',
              transition: 'all 0.2s ease',
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = 'var(--ft-color-hover, #1F1F1F)';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
            }}
          >
            {t('modals.cancel')}
          </button>
          <button
            onClick={onSell}
            style={{
              padding: '8px 16px',
              backgroundColor: colors.alert,
              color: colors.text,
              border: 'none',
              borderRadius: '2px',
              fontSize: fontSize.tiny,
              fontWeight: 700,
              cursor: 'pointer',
              transition: 'all 0.2s ease',
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}
          >
            {t('modals.sell')}
          </button>
        </div>
      </div>
    </div>
  );
};

export default SellAssetModal;
