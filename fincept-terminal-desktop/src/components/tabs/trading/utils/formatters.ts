/**
 * Formatting Utilities
 * Format prices, quantities, and P&L for display
 */

/**
 * Format price with appropriate decimal places
 */
export function formatPrice(price: number, decimals: number = 2): string {
  if (price === 0) return '0.00';
  if (!price || isNaN(price)) return '--';

  // For very small prices, use more decimals
  if (price < 0.01) {
    return price.toFixed(8);
  } else if (price < 1) {
    return price.toFixed(6);
  } else if (price < 100) {
    return price.toFixed(4);
  }

  return price.toFixed(decimals);
}

/**
 * Format price with currency symbol
 */
export function formatCurrency(amount: number, currency: string = 'USD', decimals: number = 2): string {
  if (!amount || isNaN(amount)) return '--';

  const formatted = formatPrice(Math.abs(amount), decimals);

  if (currency === 'USD') {
    return `$${formatted}`;
  }

  return `${formatted} ${currency}`;
}

/**
 * Format quantity/size
 */
export function formatQuantity(quantity: number, decimals: number = 4): string {
  if (!quantity || isNaN(quantity)) return '0.0000';

  if (quantity < 0.0001) {
    return quantity.toFixed(8);
  } else if (quantity < 1) {
    return quantity.toFixed(6);
  }

  return quantity.toFixed(decimals);
}

/**
 * Format percentage
 */
export function formatPercent(percent: number, decimals: number = 2, showSign: boolean = true): string {
  if (!percent || isNaN(percent)) return '0.00%';

  const sign = showSign && percent > 0 ? '+' : '';
  return `${sign}${percent.toFixed(decimals)}%`;
}

/**
 * Format P&L with color and sign
 */
export function formatPnL(pnl: number, decimals: number = 2): { text: string; color: string; sign: string } {
  if (!pnl || isNaN(pnl)) {
    return { text: '$0.00', color: 'gray', sign: '' };
  }

  const sign = pnl >= 0 ? '+' : '';
  const color = pnl >= 0 ? 'green' : 'red';
  const text = `${sign}$${formatPrice(Math.abs(pnl), decimals)}`;

  return { text, color, sign };
}

/**
 * Format volume (abbreviate large numbers)
 */
export function formatVolume(volume: number): string {
  if (!volume || isNaN(volume)) return '0';

  if (volume >= 1_000_000_000) {
    return `${(volume / 1_000_000_000).toFixed(2)}B`;
  } else if (volume >= 1_000_000) {
    return `${(volume / 1_000_000).toFixed(2)}M`;
  } else if (volume >= 1_000) {
    return `${(volume / 1_000).toFixed(2)}K`;
  }

  return volume.toFixed(2);
}

/**
 * Format large numbers with commas
 */
export function formatNumber(num: number, decimals: number = 2): string {
  if (!num || isNaN(num)) return '0';

  return num.toLocaleString(undefined, {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals,
  });
}

/**
 * Format timestamp to readable time
 */
export function formatTime(timestamp: number): string {
  if (!timestamp) return '--';

  const date = new Date(timestamp);
  return date.toLocaleTimeString('en-US', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false,
  });
}

/**
 * Format timestamp to readable date
 */
export function formatDate(timestamp: number): string {
  if (!timestamp) return '--';

  const date = new Date(timestamp);
  return date.toLocaleDateString('en-US', {
    year: 'numeric',
    month: 'short',
    day: 'numeric',
  });
}

/**
 * Format timestamp to full datetime
 */
export function formatDateTime(timestamp: number): string {
  if (!timestamp) return '--';

  return `${formatDate(timestamp)} ${formatTime(timestamp)}`;
}

/**
 * Format order status
 */
export function formatOrderStatus(status: string): { text: string; color: string } {
  const statusMap: Record<string, { text: string; color: string }> = {
    open: { text: 'OPEN', color: 'blue' },
    closed: { text: 'FILLED', color: 'green' },
    canceled: { text: 'CANCELED', color: 'gray' },
    expired: { text: 'EXPIRED', color: 'orange' },
    rejected: { text: 'REJECTED', color: 'red' },
    pending: { text: 'PENDING', color: 'yellow' },
  };

  return statusMap[status?.toLowerCase()] || { text: status?.toUpperCase() || 'UNKNOWN', color: 'gray' };
}

/**
 * Format order side
 */
export function formatOrderSide(side: string): { text: string; color: string } {
  const sideMap: Record<string, { text: string; color: string }> = {
    buy: { text: 'BUY', color: 'green' },
    sell: { text: 'SELL', color: 'red' },
    long: { text: 'LONG', color: 'green' },
    short: { text: 'SHORT', color: 'red' },
  };

  return sideMap[side?.toLowerCase()] || { text: side?.toUpperCase() || 'UNKNOWN', color: 'gray' };
}

/**
 * Truncate long strings
 */
export function truncate(str: string, maxLength: number = 20): string {
  if (!str) return '';
  if (str.length <= maxLength) return str;

  return `${str.substring(0, maxLength - 3)}...`;
}

/**
 * Format leverage
 */
export function formatLeverage(leverage: number): string {
  if (!leverage || leverage === 1) return '1x';
  return `${leverage}x`;
}
