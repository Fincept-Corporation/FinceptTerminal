// Portfolio Utility Functions
// Formatting, calculations, and helper functions
import { terminalThemeService } from '@/services/terminalThemeService';

export const getBloombergColors = () => {
  const theme = terminalThemeService.getTheme();
  return {
    ORANGE: theme.colors.primary,
    WHITE: theme.colors.text,
    RED: theme.colors.alert,
    GREEN: theme.colors.secondary,
    GRAY: theme.colors.textMuted,
    DARK_BG: '#1a1a1a',
    PANEL_BG: theme.colors.background,
    CYAN: theme.colors.accent,
    YELLOW: theme.colors.warning,
    BLUE: theme.colors.info,
    PURPLE: theme.colors.purple
  };
};

export const BLOOMBERG_COLORS = getBloombergColors();

// Currency symbols and formatting
const CURRENCY_SYMBOLS: Record<string, string> = {
  USD: '$',
  EUR: '€',
  GBP: '£',
  INR: '₹',
  JPY: '¥',
  CNY: '¥',
  AUD: 'A$',
  CAD: 'C$',
  CHF: 'CHF'
};

const CURRENCY_LOCALES: Record<string, string> = {
  USD: 'en-US',
  EUR: 'de-DE',
  GBP: 'en-GB',
  INR: 'en-IN',
  JPY: 'ja-JP',
  CNY: 'zh-CN',
  AUD: 'en-AU',
  CAD: 'en-CA',
  CHF: 'de-CH'
};

/**
 * Format currency value with proper symbol and locale
 */
export const formatCurrency = (value: number, currency: string = 'USD'): string => {
  const locale = CURRENCY_LOCALES[currency] || 'en-US';

  return new Intl.NumberFormat(locale, {
    style: 'currency',
    currency: currency,
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
  }).format(value);
};

/**
 * Get currency symbol
 */
export const getCurrencySymbol = (currency: string): string => {
  return CURRENCY_SYMBOLS[currency] || currency;
};

/**
 * Format number with specified decimals
 */
export const formatNumber = (value: number, decimals: number = 2): string => {
  return value.toFixed(decimals);
};

/**
 * Format percentage with sign
 */
export const formatPercent = (value: number): string => {
  return `${value >= 0 ? '+' : ''}${value.toFixed(2)}%`;
};

/**
 * Format large numbers (millions, billions)
 */
export const formatLargeNumber = (value: number, currency?: string): string => {
  const absValue = Math.abs(value);
  const symbol = currency ? getCurrencySymbol(currency) : '';

  if (absValue >= 1e9) {
    return `${symbol}${(value / 1e9).toFixed(2)}B`;
  } else if (absValue >= 1e6) {
    return `${symbol}${(value / 1e6).toFixed(2)}M`;
  } else if (absValue >= 1e3) {
    return `${symbol}${(value / 1e3).toFixed(2)}K`;
  }

  return currency ? formatCurrency(value, currency) : value.toFixed(2);
};

/**
 * Calculate percentage change
 */
export const calculatePercentChange = (current: number, previous: number): number => {
  if (previous === 0) return 0;
  return ((current - previous) / previous) * 100;
};

/**
 * Calculate annualized return
 */
export const calculateAnnualizedReturn = (totalReturn: number, days: number): number => {
  if (days === 0) return 0;
  const years = days / 365;
  return (Math.pow(1 + totalReturn / 100, 1 / years) - 1) * 100;
};

/**
 * Calculate Sharpe Ratio
 */
export const calculateSharpeRatio = (returns: number[], riskFreeRate: number = 0.03): number => {
  if (returns.length === 0) return 0;

  const avgReturn = returns.reduce((sum, r) => sum + r, 0) / returns.length;
  const variance = returns.reduce((sum, r) => sum + Math.pow(r - avgReturn, 2), 0) / returns.length;
  const stdDev = Math.sqrt(variance);

  if (stdDev === 0) return 0;

  return (avgReturn - riskFreeRate) / stdDev;
};

/**
 * Calculate Beta (market sensitivity)
 */
export const calculateBeta = (portfolioReturns: number[], marketReturns: number[]): number => {
  if (portfolioReturns.length !== marketReturns.length || portfolioReturns.length === 0) {
    return 1; // Default beta
  }

  const n = portfolioReturns.length;
  const avgPortfolio = portfolioReturns.reduce((sum, r) => sum + r, 0) / n;
  const avgMarket = marketReturns.reduce((sum, r) => sum + r, 0) / n;

  let covariance = 0;
  let marketVariance = 0;

  for (let i = 0; i < n; i++) {
    covariance += (portfolioReturns[i] - avgPortfolio) * (marketReturns[i] - avgMarket);
    marketVariance += Math.pow(marketReturns[i] - avgMarket, 2);
  }

  if (marketVariance === 0) return 1;

  return covariance / marketVariance;
};

/**
 * Calculate Volatility (annualized standard deviation)
 */
export const calculateVolatility = (returns: number[]): number => {
  if (returns.length === 0) return 0;

  const avgReturn = returns.reduce((sum, r) => sum + r, 0) / returns.length;
  const variance = returns.reduce((sum, r) => sum + Math.pow(r - avgReturn, 2), 0) / returns.length;
  const stdDev = Math.sqrt(variance);

  // Annualize (assuming daily returns)
  return stdDev * Math.sqrt(252) * 100;
};

/**
 * Calculate Maximum Drawdown
 */
export const calculateMaxDrawdown = (values: number[]): number => {
  if (values.length === 0) return 0;

  let maxDrawdown = 0;
  let peak = values[0];

  for (const value of values) {
    if (value > peak) {
      peak = value;
    }

    const drawdown = ((peak - value) / peak) * 100;
    if (drawdown > maxDrawdown) {
      maxDrawdown = drawdown;
    }
  }

  return maxDrawdown;
};

/**
 * Calculate Value at Risk (VaR) - 95% confidence
 */
export const calculateVaR = (returns: number[], confidenceLevel: number = 0.95): number => {
  if (returns.length === 0) return 0;

  const sortedReturns = [...returns].sort((a, b) => a - b);
  const index = Math.floor(sortedReturns.length * (1 - confidenceLevel));

  return sortedReturns[index] || 0;
};

/**
 * Group holdings by sector
 * DEPRECATED: Use sectorService.getSectorInfo() for real sector classification
 */
export interface SectorAllocation {
  sector: string;
  value: number;
  weight: number;
  color: string;
}

// Removed groupBySector - use sectorService instead

/**
 * Calculate tax liability (simple capital gains)
 */
export interface TaxCalculation {
  shortTermGains: number;
  longTermGains: number;
  shortTermTax: number;
  longTermTax: number;
  totalTax: number;
}

export const calculateTaxLiability = (
  transactions: any[],
  shortTermRate: number = 0.15,
  longTermRate: number = 0.20,
  holdingPeriodDays: number = 365
): TaxCalculation => {
  let shortTermGains = 0;
  let longTermGains = 0;

  // Group by symbol and calculate FIFO gains
  const symbolTransactions = new Map<string, any[]>();

  transactions.forEach(txn => {
    if (!symbolTransactions.has(txn.symbol)) {
      symbolTransactions.set(txn.symbol, []);
    }
    symbolTransactions.get(txn.symbol)!.push(txn);
  });

  symbolTransactions.forEach((txns, symbol) => {
    const buys = txns.filter(t => t.transaction_type === 'BUY').sort((a, b) =>
      new Date(a.transaction_date).getTime() - new Date(b.transaction_date).getTime()
    );
    const sells = txns.filter(t => t.transaction_type === 'SELL').sort((a, b) =>
      new Date(a.transaction_date).getTime() - new Date(b.transaction_date).getTime()
    );

    sells.forEach(sell => {
      let remainingQuantity = sell.quantity;
      const sellDate = new Date(sell.transaction_date);

      for (const buy of buys) {
        if (remainingQuantity <= 0) break;

        const quantityToMatch = Math.min(remainingQuantity, buy.quantity);
        const buyDate = new Date(buy.transaction_date);
        const daysDiff = Math.floor((sellDate.getTime() - buyDate.getTime()) / (1000 * 60 * 60 * 24));

        const gain = (sell.price - buy.price) * quantityToMatch;

        if (daysDiff > holdingPeriodDays) {
          longTermGains += gain;
        } else {
          shortTermGains += gain;
        }

        remainingQuantity -= quantityToMatch;
      }
    });
  });

  return {
    shortTermGains,
    longTermGains,
    shortTermTax: shortTermGains * shortTermRate,
    longTermTax: longTermGains * longTermRate,
    totalTax: (shortTermGains * shortTermRate) + (longTermGains * longTermRate)
  };
};

/**
 * Format date
 */
export const formatDate = (date: string | Date): string => {
  return new Date(date).toLocaleDateString('en-US', {
    year: 'numeric',
    month: 'short',
    day: 'numeric'
  });
};

/**
 * Format time
 */
export const formatTime = (date: string | Date): string => {
  return new Date(date).toLocaleTimeString('en-US', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  });
};

/**
 * Format datetime
 */
export const formatDateTime = (date: string | Date): string => {
  return `${formatDate(date)} ${formatTime(date)}`;
};

/**
 * Get color for P&L
 */
export const getPnLColor = (value: number): string => {
  return value >= 0 ? BLOOMBERG_COLORS.GREEN : BLOOMBERG_COLORS.RED;
};

/**
 * Debounce function
 */
export const debounce = <T extends (...args: any[]) => any>(
  func: T,
  wait: number
): ((...args: Parameters<T>) => void) => {
  let timeout: NodeJS.Timeout;

  return (...args: Parameters<T>) => {
    clearTimeout(timeout);
    timeout = setTimeout(() => func(...args), wait);
  };
};

/**
 * Calculate compound annual growth rate (CAGR)
 */
export const calculateCAGR = (initialValue: number, finalValue: number, years: number): number => {
  if (initialValue === 0 || years === 0) return 0;
  return (Math.pow(finalValue / initialValue, 1 / years) - 1) * 100;
};
