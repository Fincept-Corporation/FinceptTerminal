import React from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';

const CURRENCIES = [
  { code: 'USD', name: 'US Dollar' },
  { code: 'EUR', name: 'Euro' },
  { code: 'GBP', name: 'British Pound' },
  { code: 'JPY', name: 'Japanese Yen' },
  { code: 'CNY', name: 'Chinese Yuan' },
  { code: 'INR', name: 'Indian Rupee' },
  { code: 'CAD', name: 'Canadian Dollar' },
  { code: 'AUD', name: 'Australian Dollar' },
  { code: 'CHF', name: 'Swiss Franc' },
  { code: 'KRW', name: 'South Korean Won' },
  { code: 'SGD', name: 'Singapore Dollar' },
  { code: 'HKD', name: 'Hong Kong Dollar' },
  { code: 'SEK', name: 'Swedish Krona' },
  { code: 'NOK', name: 'Norwegian Krone' },
  { code: 'DKK', name: 'Danish Krone' },
  { code: 'NZD', name: 'New Zealand Dollar' },
  { code: 'MXN', name: 'Mexican Peso' },
  { code: 'BRL', name: 'Brazilian Real' },
  { code: 'ZAR', name: 'South African Rand' },
  { code: 'RUB', name: 'Russian Ruble' },
  { code: 'TRY', name: 'Turkish Lira' },
  { code: 'PLN', name: 'Polish Zloty' },
  { code: 'THB', name: 'Thai Baht' },
  { code: 'IDR', name: 'Indonesian Rupiah' },
  { code: 'MYR', name: 'Malaysian Ringgit' },
  { code: 'PHP', name: 'Philippine Peso' },
  { code: 'AED', name: 'UAE Dirham' },
  { code: 'SAR', name: 'Saudi Riyal' },
  { code: 'ILS', name: 'Israeli Shekel' },
  { code: 'ARS', name: 'Argentine Peso' },
  { code: 'CLP', name: 'Chilean Peso' },
  { code: 'COP', name: 'Colombian Peso' },
  { code: 'CZK', name: 'Czech Koruna' },
  { code: 'HUF', name: 'Hungarian Forint' },
  { code: 'RON', name: 'Romanian Leu' },
] as const;

interface CreatePortfolioModalProps {
  show: boolean;
  formState: {
    name: string;
    owner: string;
    currency: string;
  };
  onNameChange: (value: string) => void;
  onOwnerChange: (value: string) => void;
  onCurrencyChange: (value: string) => void;
  onClose: () => void;
  onCreate: () => void;
}

const CreatePortfolioModal: React.FC<CreatePortfolioModalProps> = ({
  show,
  formState,
  onNameChange,
  onOwnerChange,
  onCurrencyChange,
  onClose,
  onCreate
}) => {
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();

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
        border: `2px solid ${colors.primary}`,
        padding: '24px',
        minWidth: '400px',
        maxWidth: '600px',
        boxShadow: `0 4px 16px ${colors.background}80`,
        fontFamily,
      }}>
        <div style={{
          color: colors.primary,
          fontSize: fontSize.heading,
          fontWeight: 700,
          marginBottom: '16px',
          letterSpacing: '0.5px',
        }}>
          {t('modals.createNew')}
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
            {t('modals.portfolioName')} *
          </label>
          <input
            type="text"
            value={formState.name}
            onChange={(e) => onNameChange(e.target.value)}
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
            placeholder="My Portfolio"
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
            {t('modals.owner')} *
          </label>
          <input
            type="text"
            value={formState.owner}
            onChange={(e) => onOwnerChange(e.target.value)}
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
            placeholder="John Doe"
          />
        </div>

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
            {t('modals.currency')}
          </label>
          <select
            value={formState.currency}
            onChange={(e) => onCurrencyChange(e.target.value)}
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: colors.background,
              color: colors.text,
              border: '1px solid var(--ft-color-border, #2A2A2A)',
              borderRadius: '2px',
              fontSize: fontSize.small,
              fontFamily,
              cursor: 'pointer',
            }}
          >
            {CURRENCIES.map(({ code, name }) => (
              <option key={code} value={code}>
                {code} - {name}
              </option>
            ))}
          </select>
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
            onClick={onCreate}
            style={{
              padding: '8px 16px',
              backgroundColor: colors.primary,
              color: colors.background,
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
            {t('modals.create')}
          </button>
        </div>
      </div>
    </div>
  );
};

export default CreatePortfolioModal;
