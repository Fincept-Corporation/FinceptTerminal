/**
 * Market Hours Service
 *
 * Provides utilities for checking market hours across different exchanges.
 * Used to optimize polling strategies - poll frequently when market is open,
 * use cached data when market is closed.
 */

export type MarketStatus = 'PRE_OPEN' | 'OPEN' | 'CLOSED' | 'POST_MARKET' | 'HOLIDAY';

export interface MarketHoursConfig {
  open: { hour: number; minute: number };
  close: { hour: number; minute: number };
  preMarketOpen?: { hour: number; minute: number };
  preMarketClose?: { hour: number; minute: number };
  postMarketOpen?: { hour: number; minute: number };
  postMarketClose?: { hour: number; minute: number };
  timezone: string;
  tradingDays: number[]; // 0 = Sunday, 1 = Monday, ..., 6 = Saturday
}

export interface MarketStatusInfo {
  status: MarketStatus;
  isOpen: boolean;
  nextOpen?: Date;
  nextClose?: Date;
  currentTime: Date;
  exchangeTime: Date;
}

// Market hours configuration for different exchanges
const MARKET_HOURS: Record<string, MarketHoursConfig> = {
  // Indian Markets (NSE, BSE)
  NSE: {
    open: { hour: 9, minute: 15 },
    close: { hour: 15, minute: 30 },
    preMarketOpen: { hour: 9, minute: 0 },
    preMarketClose: { hour: 9, minute: 15 },
    postMarketOpen: { hour: 15, minute: 40 },
    postMarketClose: { hour: 16, minute: 0 },
    timezone: 'Asia/Kolkata',
    tradingDays: [1, 2, 3, 4, 5], // Monday to Friday
  },
  BSE: {
    open: { hour: 9, minute: 15 },
    close: { hour: 15, minute: 30 },
    preMarketOpen: { hour: 9, minute: 0 },
    preMarketClose: { hour: 9, minute: 15 },
    postMarketOpen: { hour: 15, minute: 40 },
    postMarketClose: { hour: 16, minute: 0 },
    timezone: 'Asia/Kolkata',
    tradingDays: [1, 2, 3, 4, 5],
  },
  // US Markets
  NYSE: {
    open: { hour: 9, minute: 30 },
    close: { hour: 16, minute: 0 },
    preMarketOpen: { hour: 4, minute: 0 },
    preMarketClose: { hour: 9, minute: 30 },
    postMarketOpen: { hour: 16, minute: 0 },
    postMarketClose: { hour: 20, minute: 0 },
    timezone: 'America/New_York',
    tradingDays: [1, 2, 3, 4, 5],
  },
  NASDAQ: {
    open: { hour: 9, minute: 30 },
    close: { hour: 16, minute: 0 },
    preMarketOpen: { hour: 4, minute: 0 },
    preMarketClose: { hour: 9, minute: 30 },
    postMarketOpen: { hour: 16, minute: 0 },
    postMarketClose: { hour: 20, minute: 0 },
    timezone: 'America/New_York',
    tradingDays: [1, 2, 3, 4, 5],
  },
  // Crypto Markets (24/7)
  CRYPTO: {
    open: { hour: 0, minute: 0 },
    close: { hour: 23, minute: 59 },
    timezone: 'UTC',
    tradingDays: [0, 1, 2, 3, 4, 5, 6], // All days
  },
};

/**
 * Get current time in a specific timezone
 */
function getTimeInTimezone(timezone: string): Date {
  const now = new Date();
  const options: Intl.DateTimeFormatOptions = {
    timeZone: timezone,
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false,
  };

  const formatter = new Intl.DateTimeFormat('en-US', options);
  const parts = formatter.formatToParts(now);

  const getPart = (type: string) => parts.find(p => p.type === type)?.value || '0';

  return new Date(
    parseInt(getPart('year')),
    parseInt(getPart('month')) - 1,
    parseInt(getPart('day')),
    parseInt(getPart('hour')),
    parseInt(getPart('minute')),
    parseInt(getPart('second'))
  );
}

/**
 * Convert hours and minutes to total minutes since midnight
 */
function toMinutes(hour: number, minute: number): number {
  return hour * 60 + minute;
}

/**
 * Check if current time is within a time range
 */
function isWithinRange(
  currentMinutes: number,
  start: { hour: number; minute: number },
  end: { hour: number; minute: number }
): boolean {
  const startMinutes = toMinutes(start.hour, start.minute);
  const endMinutes = toMinutes(end.hour, end.minute);
  return currentMinutes >= startMinutes && currentMinutes < endMinutes;
}

/**
 * Get market status for a specific exchange
 */
export function getMarketStatus(exchange: string): MarketStatusInfo {
  const config = MARKET_HOURS[exchange.toUpperCase()] || MARKET_HOURS.NSE;
  const now = new Date();
  const exchangeTime = getTimeInTimezone(config.timezone);

  const dayOfWeek = exchangeTime.getDay();
  const currentMinutes = toMinutes(exchangeTime.getHours(), exchangeTime.getMinutes());

  // Check if it's a trading day
  if (!config.tradingDays.includes(dayOfWeek)) {
    return {
      status: 'CLOSED',
      isOpen: false,
      currentTime: now,
      exchangeTime,
    };
  }

  // Check pre-market
  if (config.preMarketOpen && config.preMarketClose) {
    if (isWithinRange(currentMinutes, config.preMarketOpen, config.preMarketClose)) {
      return {
        status: 'PRE_OPEN',
        isOpen: false,
        currentTime: now,
        exchangeTime,
      };
    }
  }

  // Check market hours
  if (isWithinRange(currentMinutes, config.open, config.close)) {
    return {
      status: 'OPEN',
      isOpen: true,
      currentTime: now,
      exchangeTime,
    };
  }

  // Check post-market
  if (config.postMarketOpen && config.postMarketClose) {
    if (isWithinRange(currentMinutes, config.postMarketOpen, config.postMarketClose)) {
      return {
        status: 'POST_MARKET',
        isOpen: false,
        currentTime: now,
        exchangeTime,
      };
    }
  }

  return {
    status: 'CLOSED',
    isOpen: false,
    currentTime: now,
    exchangeTime,
  };
}

/**
 * Check if market is currently open
 */
export function isMarketOpen(exchange: string): boolean {
  return getMarketStatus(exchange).isOpen;
}

/**
 * Check if market is in extended hours (pre-market or post-market)
 */
export function isExtendedHours(exchange: string): boolean {
  const status = getMarketStatus(exchange).status;
  return status === 'PRE_OPEN' || status === 'POST_MARKET';
}

/**
 * Get recommended polling interval based on market status
 * Returns interval in milliseconds
 */
export function getPollingInterval(exchange: string): number {
  const { status } = getMarketStatus(exchange);

  switch (status) {
    case 'OPEN':
      // Market is open - poll every 30 seconds
      return 30 * 1000;
    case 'PRE_OPEN':
      // Pre-market - poll every 2 minutes
      return 2 * 60 * 1000;
    case 'POST_MARKET':
      // Post-market - poll every 5 minutes
      return 5 * 60 * 1000;
    case 'CLOSED':
    case 'HOLIDAY':
    default:
      // Market closed - poll every 30 minutes (data won't change)
      return 30 * 60 * 1000;
  }
}

/**
 * Get recommended cache TTL based on market status
 * Returns TTL in seconds
 */
export function getCacheTTL(exchange: string): number {
  const { status } = getMarketStatus(exchange);

  switch (status) {
    case 'OPEN':
      // Market is open - cache for 1 minute
      return 60;
    case 'PRE_OPEN':
      // Pre-market - cache for 2 minutes
      return 2 * 60;
    case 'POST_MARKET':
      // Post-market - cache for 5 minutes
      return 5 * 60;
    case 'CLOSED':
    case 'HOLIDAY':
    default:
      // Market closed - cache for 30 minutes (data won't change)
      return 30 * 60;
  }
}

/**
 * Check if we should skip polling (market closed and have cached data)
 */
export function shouldSkipPolling(exchange: string, hasCachedData: boolean): boolean {
  const { status } = getMarketStatus(exchange);

  // If market is closed and we have cached data, skip polling
  if (status === 'CLOSED' && hasCachedData) {
    return true;
  }

  return false;
}

/**
 * Get human-readable market status message
 */
export function getMarketStatusMessage(exchange: string): string {
  const { status, exchangeTime } = getMarketStatus(exchange);
  const config = MARKET_HOURS[exchange.toUpperCase()] || MARKET_HOURS.NSE;

  const formatTime = (t: { hour: number; minute: number }) =>
    `${t.hour.toString().padStart(2, '0')}:${t.minute.toString().padStart(2, '0')}`;

  switch (status) {
    case 'OPEN':
      return `Market Open (closes at ${formatTime(config.close)})`;
    case 'PRE_OPEN':
      return `Pre-Market (opens at ${formatTime(config.open)})`;
    case 'POST_MARKET':
      return `Post-Market (closes at ${formatTime(config.postMarketClose!)})`;
    case 'CLOSED':
      return `Market Closed (opens at ${formatTime(config.open)})`;
    case 'HOLIDAY':
      return 'Market Holiday';
    default:
      return 'Unknown';
  }
}

// Export default instance for convenience
export default {
  getMarketStatus,
  isMarketOpen,
  isExtendedHours,
  getPollingInterval,
  getCacheTTL,
  shouldSkipPolling,
  getMarketStatusMessage,
};
