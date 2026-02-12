import React, { useState, useEffect, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { Transaction } from '../../../../services/portfolio/portfolioService';
import { invoke } from '@tauri-apps/api/core';

interface EditTransactionModalProps {
  show: boolean;
  transaction: Transaction | null;
  onClose: () => void;
  onSave: (transactionId: string, quantity: number, price: number, transactionDate: string, notes?: string) => void;
  onDelete: (transactionId: string) => void;
}

const EditTransactionModal: React.FC<EditTransactionModalProps> = ({
  show,
  transaction,
  onClose,
  onSave,
  onDelete
}) => {
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  const [quantity, setQuantity] = useState('');
  const [price, setPrice] = useState('');
  const [transactionDate, setTransactionDate] = useState('');
  const [notes, setNotes] = useState('');
  const [showDeleteConfirm, setShowDeleteConfirm] = useState(false);
  const [fetchingPrice, setFetchingPrice] = useState(false);
  const [priceSource, setPriceSource] = useState<'auto' | 'manual' | null>(null);
  const [priceError, setPriceError] = useState<string | null>(null);
  const initialDateRef = useRef<string>('');

  // Initialize form when transaction changes
  useEffect(() => {
    if (transaction) {
      setQuantity(transaction.quantity.toString());
      setPrice(transaction.price.toString());
      setPriceSource(null);
      setPriceError(null);
      // Parse date - handle both ISO format and datetime format
      const dateStr = transaction.transaction_date;
      if (dateStr) {
        // Convert to YYYY-MM-DDTHH:mm format for datetime-local input
        const date = new Date(dateStr);
        if (!isNaN(date.getTime())) {
          const localDate = new Date(date.getTime() - date.getTimezoneOffset() * 60000);
          const formattedDate = localDate.toISOString().slice(0, 16);
          setTransactionDate(formattedDate);
          initialDateRef.current = formattedDate.slice(0, 10); // Store initial date (YYYY-MM-DD)
        } else {
          setTransactionDate(dateStr.slice(0, 16));
          initialDateRef.current = dateStr.slice(0, 10);
        }
      }
      setNotes(transaction.notes || '');
    }
  }, [transaction]);

  // Auto-fetch historical price when date changes
  useEffect(() => {
    if (!transaction || !transactionDate) return;

    const currentDateOnly = transactionDate.slice(0, 10); // YYYY-MM-DD

    // Only fetch if date actually changed from initial
    if (currentDateOnly === initialDateRef.current) {
      return;
    }

    const fetchHistoricalPrice = async () => {
      setFetchingPrice(true);
      setPriceError(null);

      try {
        const result = await invoke<string>('execute_yfinance_command', {
          command: 'historical_price',
          args: [transaction.symbol, currentDateOnly],
        });

        const parsed = JSON.parse(result);

        if (parsed.found && parsed.price) {
          setPrice(parsed.price.toString());
          setPriceSource('auto');
          if (parsed.date !== currentDateOnly) {
            setPriceError(`Using price from ${parsed.date} (closest available)`);
          }
        } else {
          setPriceError(parsed.error || 'Price not found');
          setPriceSource(null);
        }
      } catch (error) {
        console.error('Failed to fetch historical price:', error);
        setPriceError('Failed to fetch price');
        setPriceSource(null);
      } finally {
        setFetchingPrice(false);
      }
    };

    // Debounce: wait 500ms after user stops changing date
    const timer = setTimeout(fetchHistoricalPrice, 500);
    return () => clearTimeout(timer);
  }, [transactionDate, transaction]);

  if (!show || !transaction) return null;

  const handleSave = () => {
    const qty = parseFloat(quantity);
    const prc = parseFloat(price);

    if (isNaN(qty) || isNaN(prc) || qty <= 0 || prc <= 0) {
      return;
    }

    // Convert local datetime to ISO format
    const date = new Date(transactionDate);
    const isoDate = date.toISOString();

    onSave(transaction.id, qty, prc, isoDate, notes || undefined);
    onClose();
  };

  const handleDelete = () => {
    onDelete(transaction.id);
    setShowDeleteConfirm(false);
    onClose();
  };

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
        border: `2px solid ${colors.primary}`,
        padding: '24px',
        minWidth: '450px',
        maxWidth: '600px',
        boxShadow: `0 4px 16px ${colors.background}80`,
        fontFamily,
      }}>
        {showDeleteConfirm ? (
          // Delete confirmation view
          <>
            <div style={{
              color: colors.alert,
              fontSize: fontSize.heading,
              fontWeight: 700,
              marginBottom: '16px',
              letterSpacing: '0.5px',
            }}>
              {t('modals.confirmDelete', 'CONFIRM DELETE')}
            </div>
            <p style={{ color: colors.text, marginBottom: '16px', fontSize: fontSize.small }}>
              {t('modals.deleteTransactionWarning', 'Are you sure you want to delete this transaction? This action cannot be undone.')}
            </p>
            <div style={{
              backgroundColor: colors.background,
              padding: '12px',
              marginBottom: '16px',
              border: '1px solid var(--ft-color-border, #2A2A2A)',
            }}>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, marginBottom: '4px' }}>
                {transaction.transaction_type} - {transaction.symbol}
              </div>
              <div style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.small }}>
                {transaction.quantity} @ ${transaction.price.toFixed(2)} = ${transaction.total_value.toFixed(2)}
              </div>
            </div>
            <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
              <button
                onClick={() => setShowDeleteConfirm(false)}
                style={{
                  padding: '6px 12px',
                  backgroundColor: 'transparent',
                  border: '1px solid var(--ft-color-border, #2A2A2A)',
                  color: colors.textMuted,
                  borderRadius: '2px',
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  cursor: 'pointer',
                  textTransform: 'uppercase',
                }}
              >
                {t('modals.cancel', 'CANCEL')}
              </button>
              <button
                onClick={handleDelete}
                style={{
                  padding: '6px 12px',
                  backgroundColor: colors.alert,
                  color: '#fff',
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  cursor: 'pointer',
                  textTransform: 'uppercase',
                }}
              >
                {t('modals.delete', 'DELETE')}
              </button>
            </div>
          </>
        ) : (
          // Edit form view
          <>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              marginBottom: '16px',
            }}>
              <div style={{
                color: colors.primary,
                fontSize: fontSize.heading,
                fontWeight: 700,
                letterSpacing: '0.5px',
              }}>
                {t('modals.editTransaction', 'EDIT TRANSACTION')}
              </div>
              <span style={{
                padding: '2px 8px',
                backgroundColor: transaction.transaction_type === 'BUY' ? `${colors.success}20` : `${colors.alert}20`,
                color: transaction.transaction_type === 'BUY' ? colors.success : colors.alert,
                fontSize: fontSize.tiny,
                fontWeight: 700,
                borderRadius: '2px',
              }}>
                {transaction.transaction_type}
              </span>
            </div>

            {/* Symbol (read-only) */}
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
                {t('modals.symbol', 'SYMBOL')}
              </label>
              <div style={{
                padding: '8px 10px',
                backgroundColor: colors.background,
                color: 'var(--ft-color-accent, #00E5FF)',
                border: '1px solid var(--ft-color-border, #2A2A2A)',
                borderRadius: '2px',
                fontSize: fontSize.small,
                fontFamily,
                fontWeight: 700,
              }}>
                {transaction.symbol}
              </div>
            </div>

            {/* Quantity */}
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
                {t('modals.quantity', 'QUANTITY')} *
              </label>
              <input
                type="text"
                inputMode="decimal"
                value={quantity}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setQuantity(v); }}
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
                onFocus={(e) => { e.currentTarget.style.borderColor = colors.primary; }}
                onBlur={(e) => { e.currentTarget.style.borderColor = 'var(--ft-color-border, #2A2A2A)'; }}
              />
            </div>

            {/* Price */}
            <div style={{ marginBottom: '12px' }}>
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginBottom: '4px',
              }}>
                <label style={{
                  color: colors.textMuted,
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                }}>
                  {t('modals.price', 'PRICE')} *
                </label>
                {fetchingPrice && (
                  <span style={{ color: 'var(--ft-color-warning, #FFD700)', fontSize: fontSize.tiny }}>
                    ● Fetching...
                  </span>
                )}
                {!fetchingPrice && priceSource === 'auto' && !priceError && (
                  <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.tiny }}>
                    ✓ Auto-fetched
                  </span>
                )}
                {!fetchingPrice && priceError && (
                  <span style={{ color: priceSource === 'auto' ? 'var(--ft-color-warning, #FFD700)' : colors.alert, fontSize: fontSize.tiny }}>
                    {priceError}
                  </span>
                )}
              </div>
              <input
                type="text"
                inputMode="decimal"
                value={price}
                onChange={(e) => {
                  const v = e.target.value;
                  if (v === '' || /^\d*\.?\d*$/.test(v)) {
                    setPrice(v);
                    setPriceSource('manual');
                  }
                }}
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
                onFocus={(e) => { e.currentTarget.style.borderColor = colors.primary; }}
                onBlur={(e) => { e.currentTarget.style.borderColor = fetchingPrice ? 'var(--ft-color-warning, #FFD700)' : 'var(--ft-color-border, #2A2A2A)'; }}
              />
            </div>

            {/* Transaction Date */}
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
                {t('modals.transactionDate', 'TRANSACTION DATE')} *
              </label>
              <input
                type="datetime-local"
                value={transactionDate}
                onChange={(e) => setTransactionDate(e.target.value)}
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
                onFocus={(e) => { e.currentTarget.style.borderColor = colors.primary; }}
                onBlur={(e) => { e.currentTarget.style.borderColor = 'var(--ft-color-border, #2A2A2A)'; }}
              />
            </div>

            {/* Notes */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{
                color: colors.textMuted,
                fontSize: fontSize.tiny,
                fontWeight: 700,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                display: 'block',
                marginBottom: '4px',
              }}>
                {t('modals.notes', 'NOTES')}
              </label>
              <input
                type="text"
                value={notes}
                onChange={(e) => setNotes(e.target.value)}
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
                onFocus={(e) => { e.currentTarget.style.borderColor = colors.primary; }}
                onBlur={(e) => { e.currentTarget.style.borderColor = 'var(--ft-color-border, #2A2A2A)'; }}
                placeholder={t('modals.notesPlaceholder', 'Optional notes...')}
              />
            </div>

            {/* Calculated Total */}
            <div style={{
              backgroundColor: colors.background,
              padding: '12px',
              marginBottom: '16px',
              border: '1px solid var(--ft-color-border, #2A2A2A)',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
            }}>
              <span style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700 }}>
                {t('modals.totalValue', 'TOTAL VALUE')}
              </span>
              <span style={{ color: 'var(--ft-color-warning, #FFD700)', fontSize: fontSize.body, fontWeight: 700 }}>
                ${(parseFloat(quantity || '0') * parseFloat(price || '0')).toFixed(2)}
              </span>
            </div>

            {/* Action Buttons */}
            <div style={{ display: 'flex', gap: '8px', justifyContent: 'space-between' }}>
              <button
                onClick={() => setShowDeleteConfirm(true)}
                style={{
                  padding: '6px 12px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${colors.alert}`,
                  color: colors.alert,
                  borderRadius: '2px',
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  cursor: 'pointer',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                }}
              >
                {t('modals.delete', 'DELETE')}
              </button>
              <div style={{ display: 'flex', gap: '8px' }}>
                <button
                  onClick={onClose}
                  style={{
                    padding: '6px 12px',
                    backgroundColor: 'transparent',
                    border: '1px solid var(--ft-color-border, #2A2A2A)',
                    color: colors.textMuted,
                    borderRadius: '2px',
                    fontSize: fontSize.tiny,
                    fontWeight: 700,
                    cursor: 'pointer',
                    textTransform: 'uppercase',
                    letterSpacing: '0.5px',
                  }}
                >
                  {t('modals.cancel', 'CANCEL')}
                </button>
                <button
                  onClick={handleSave}
                  style={{
                    padding: '6px 16px',
                    backgroundColor: colors.primary,
                    color: colors.background,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: fontSize.tiny,
                    fontWeight: 700,
                    cursor: 'pointer',
                    textTransform: 'uppercase',
                    letterSpacing: '0.5px',
                  }}
                >
                  {t('modals.save', 'SAVE')}
                </button>
              </div>
            </div>
          </>
        )}
      </div>
    </div>
  );
};

export default EditTransactionModal;
