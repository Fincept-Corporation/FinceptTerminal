/**
 * Market utilities for Indian stock markets
 * Handles market timing, holidays, and trading sessions
 */

import type { IndianExchange } from '../types';
import { MARKET_TIMING } from '../constants';

// ============================================================================
// Timezone Utilities
// ============================================================================

const IST_OFFSET = 5.5 * 60 * 60 * 1000; // IST is UTC+5:30

/**
 * Get current time in IST
 */
export function getISTTime(): Date {
  const now = new Date();
  const utc = now.getTime() + now.getTimezoneOffset() * 60 * 1000;
  return new Date(utc + IST_OFFSET);
}

/**
 * Convert any date to IST
 */
export function toIST(date: Date): Date {
  const utc = date.getTime() + date.getTimezoneOffset() * 60 * 1000;
  return new Date(utc + IST_OFFSET);
}

/**
 * Parse time string (HH:MM) to minutes from midnight
 */
function parseTimeToMinutes(timeStr: string): number {
  const [hours, minutes] = timeStr.split(':').map(Number);
  return hours * 60 + minutes;
}

/**
 * Get minutes from midnight for a date
 */
function getMinutesFromMidnight(date: Date): number {
  return date.getHours() * 60 + date.getMinutes();
}

// ============================================================================
// Market Status
// ============================================================================

export type MarketStatus =
  | 'PRE_OPEN'
  | 'OPEN'
  | 'POST_CLOSE'
  | 'CLOSED'
  | 'HOLIDAY';

export interface MarketStatusInfo {
  status: MarketStatus;
  message: string;
  nextOpenTime?: Date;
  closingTime?: Date;
}

/**
 * Check if a date is a weekend (Saturday or Sunday)
 */
export function isWeekend(date: Date): boolean {
  const day = date.getDay();
  return day === 0 || day === 6;
}

/**
 * Get market status for NSE/BSE
 */
export function getEquityMarketStatus(date?: Date): MarketStatusInfo {
  const istNow = date ? toIST(date) : getISTTime();
  const currentMinutes = getMinutesFromMidnight(istNow);

  // Check weekend
  if (isWeekend(istNow)) {
    return {
      status: 'CLOSED',
      message: 'Market closed (Weekend)',
      nextOpenTime: getNextTradingDay(istNow),
    };
  }

  const { preOpen, normal, postClose } = MARKET_TIMING.NSE;

  const preOpenStart = parseTimeToMinutes(preOpen.start);
  const preOpenEnd = parseTimeToMinutes(preOpen.end);
  const normalStart = parseTimeToMinutes(normal.start);
  const normalEnd = parseTimeToMinutes(normal.end);
  const postCloseStart = parseTimeToMinutes(postClose.start);
  const postCloseEnd = parseTimeToMinutes(postClose.end);

  // Pre-open session
  if (currentMinutes >= preOpenStart && currentMinutes < preOpenEnd) {
    return {
      status: 'PRE_OPEN',
      message: 'Pre-open session',
      closingTime: createTimeOnDate(istNow, preOpen.end),
    };
  }

  // Normal trading hours
  if (currentMinutes >= normalStart && currentMinutes < normalEnd) {
    return {
      status: 'OPEN',
      message: 'Market open',
      closingTime: createTimeOnDate(istNow, normal.end),
    };
  }

  // Post-close session
  if (currentMinutes >= postCloseStart && currentMinutes < postCloseEnd) {
    return {
      status: 'POST_CLOSE',
      message: 'Post-close session',
      closingTime: createTimeOnDate(istNow, postClose.end),
    };
  }

  // Market closed
  const message =
    currentMinutes < preOpenStart
      ? 'Market opens at 9:00 AM'
      : 'Market closed for the day';

  return {
    status: 'CLOSED',
    message,
    nextOpenTime:
      currentMinutes < preOpenStart
        ? createTimeOnDate(istNow, preOpen.start)
        : getNextTradingDay(istNow),
  };
}

/**
 * Get market status for MCX
 */
export function getMCXMarketStatus(date?: Date): MarketStatusInfo {
  const istNow = date ? toIST(date) : getISTTime();
  const currentMinutes = getMinutesFromMidnight(istNow);
  const day = istNow.getDay();

  // MCX is closed on Sundays
  if (day === 0) {
    return {
      status: 'CLOSED',
      message: 'Market closed (Sunday)',
      nextOpenTime: getNextTradingDay(istNow),
    };
  }

  const { morning, evening } = MARKET_TIMING.MCX;
  const morningStart = parseTimeToMinutes(morning.start);
  const eveningEnd = parseTimeToMinutes(evening.end);

  // Morning session (9:00 - 17:00)
  if (currentMinutes >= morningStart && currentMinutes < parseTimeToMinutes(morning.end)) {
    return {
      status: 'OPEN',
      message: 'Morning session',
      closingTime: createTimeOnDate(istNow, evening.end),
    };
  }

  // Evening session (17:00 - 23:30)
  if (currentMinutes >= parseTimeToMinutes(evening.start) && currentMinutes < eveningEnd) {
    return {
      status: 'OPEN',
      message: 'Evening session',
      closingTime: createTimeOnDate(istNow, evening.end),
    };
  }

  // On Saturday, MCX closes at 11:30 PM
  if (day === 6 && currentMinutes >= eveningEnd) {
    return {
      status: 'CLOSED',
      message: 'Market closed for weekend',
      nextOpenTime: getNextTradingDay(istNow),
    };
  }

  return {
    status: 'CLOSED',
    message: 'Market closed',
    nextOpenTime:
      currentMinutes < morningStart
        ? createTimeOnDate(istNow, morning.start)
        : getNextTradingDay(istNow),
  };
}

/**
 * Get market status for an exchange
 */
export function getMarketStatus(exchange: IndianExchange, date?: Date): MarketStatusInfo {
  switch (exchange) {
    case 'NSE':
    case 'BSE':
    case 'NFO':
    case 'BFO':
    case 'NSE_INDEX':
    case 'BSE_INDEX':
      return getEquityMarketStatus(date);

    case 'MCX':
    case 'MCX_INDEX':
      return getMCXMarketStatus(date);

    case 'CDS':
    case 'BCD':
    case 'CDS_INDEX':
      return getCDSMarketStatus(date);

    default:
      return getEquityMarketStatus(date);
  }
}

/**
 * Get market status for Currency (CDS)
 */
export function getCDSMarketStatus(date?: Date): MarketStatusInfo {
  const istNow = date ? toIST(date) : getISTTime();
  const currentMinutes = getMinutesFromMidnight(istNow);

  if (isWeekend(istNow)) {
    return {
      status: 'CLOSED',
      message: 'Market closed (Weekend)',
      nextOpenTime: getNextTradingDay(istNow),
    };
  }

  const { normal } = MARKET_TIMING.CDS;
  const start = parseTimeToMinutes(normal.start);
  const end = parseTimeToMinutes(normal.end);

  if (currentMinutes >= start && currentMinutes < end) {
    return {
      status: 'OPEN',
      message: 'Currency market open',
      closingTime: createTimeOnDate(istNow, normal.end),
    };
  }

  return {
    status: 'CLOSED',
    message: 'Currency market closed',
    nextOpenTime:
      currentMinutes < start
        ? createTimeOnDate(istNow, normal.start)
        : getNextTradingDay(istNow),
  };
}

/**
 * Check if market is open for trading
 */
export function isMarketOpen(exchange: IndianExchange, date?: Date): boolean {
  const status = getMarketStatus(exchange, date);
  return status.status === 'OPEN' || status.status === 'PRE_OPEN';
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Create a date with specific time on the same day
 */
function createTimeOnDate(date: Date, timeStr: string): Date {
  const [hours, minutes] = timeStr.split(':').map(Number);
  const result = new Date(date);
  result.setHours(hours, minutes, 0, 0);
  return result;
}

/**
 * Get the next trading day
 */
export function getNextTradingDay(fromDate: Date): Date {
  const result = new Date(fromDate);
  result.setDate(result.getDate() + 1);

  // Skip weekends
  while (isWeekend(result)) {
    result.setDate(result.getDate() + 1);
  }

  // Set to market open time (9:00 AM IST)
  result.setHours(9, 0, 0, 0);
  return result;
}

/**
 * Get previous trading day
 */
export function getPreviousTradingDay(fromDate: Date): Date {
  const result = new Date(fromDate);
  result.setDate(result.getDate() - 1);

  // Skip weekends
  while (isWeekend(result)) {
    result.setDate(result.getDate() - 1);
  }

  return result;
}

// ============================================================================
// Time Formatting
// ============================================================================

/**
 * Format time remaining until market close
 */
export function formatTimeRemaining(ms: number): string {
  const hours = Math.floor(ms / (1000 * 60 * 60));
  const minutes = Math.floor((ms % (1000 * 60 * 60)) / (1000 * 60));

  if (hours > 0) {
    return `${hours}h ${minutes}m`;
  }
  return `${minutes}m`;
}

/**
 * Get time until market close
 */
export function getTimeUntilClose(exchange: IndianExchange): number | null {
  const status = getMarketStatus(exchange);
  if (status.status !== 'OPEN' || !status.closingTime) {
    return null;
  }

  const now = getISTTime();
  return status.closingTime.getTime() - now.getTime();
}

/**
 * Check if we're in the last N minutes of trading
 */
export function isNearClose(exchange: IndianExchange, minutesBefore: number = 15): boolean {
  const timeRemaining = getTimeUntilClose(exchange);
  if (timeRemaining === null) return false;
  return timeRemaining <= minutesBefore * 60 * 1000;
}

// ============================================================================
// Session Types
// ============================================================================

export type TradingSession = 'REGULAR' | 'AMO' | 'PRE_MARKET' | 'POST_MARKET';

/**
 * Get current trading session
 */
export function getCurrentSession(exchange: IndianExchange): TradingSession {
  const status = getMarketStatus(exchange);

  switch (status.status) {
    case 'PRE_OPEN':
      return 'PRE_MARKET';
    case 'OPEN':
      return 'REGULAR';
    case 'POST_CLOSE':
      return 'POST_MARKET';
    case 'CLOSED':
    case 'HOLIDAY':
      return 'AMO';
    default:
      return 'REGULAR';
  }
}

/**
 * Check if AMO (After Market Orders) are allowed
 */
export function isAMOAllowed(exchange: IndianExchange): boolean {
  const status = getMarketStatus(exchange);
  // AMO typically allowed between 3:45 PM and 8:57 AM next day
  return status.status === 'CLOSED' || status.status === 'POST_CLOSE';
}
