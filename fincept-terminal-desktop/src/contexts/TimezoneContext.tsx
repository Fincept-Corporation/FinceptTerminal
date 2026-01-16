/**
 * TimezoneContext.tsx
 *
 * Global timezone state management for Fincept Terminal.
 * Provides dual-timezone support:
 * - Default Timezone: Persisted in localStorage, used by navigation bar
 * - Session Timezone: Per-session, used by dashboard widgets
 */

import React, { createContext, useContext, useState, useEffect, useCallback, ReactNode } from 'react';
import { saveSetting, getSetting } from '@/services/core/sqliteService';

// ============================================================================
// Types & Constants
// ============================================================================

export interface TimezoneOption {
  id: string;
  label: string;
  shortLabel: string;
  zone: string | undefined; // undefined = use system locale
}

export const TIMEZONE_OPTIONS: TimezoneOption[] = [
  // System
  { id: 'local', label: 'Local (System)', shortLabel: 'LOC', zone: undefined },
  { id: 'utc', label: 'UTC (Coordinated)', shortLabel: 'UTC', zone: 'UTC' },

  // Americas
  { id: 'new-york', label: 'New York (EST/EDT)', shortLabel: 'NYC', zone: 'America/New_York' },
  { id: 'chicago', label: 'Chicago (CST/CDT)', shortLabel: 'CHI', zone: 'America/Chicago' },
  { id: 'denver', label: 'Denver (MST/MDT)', shortLabel: 'DEN', zone: 'America/Denver' },
  { id: 'los-angeles', label: 'Los Angeles (PST/PDT)', shortLabel: 'LAX', zone: 'America/Los_Angeles' },
  { id: 'toronto', label: 'Toronto (EST/EDT)', shortLabel: 'TOR', zone: 'America/Toronto' },
  { id: 'vancouver', label: 'Vancouver (PST/PDT)', shortLabel: 'VAN', zone: 'America/Vancouver' },
  { id: 'mexico-city', label: 'Mexico City (CST)', shortLabel: 'MEX', zone: 'America/Mexico_City' },
  { id: 'sao-paulo', label: 'São Paulo (BRT)', shortLabel: 'SAO', zone: 'America/Sao_Paulo' },
  { id: 'buenos-aires', label: 'Buenos Aires (ART)', shortLabel: 'BUE', zone: 'America/Buenos_Aires' },

  // Europe
  { id: 'london', label: 'London (GMT/BST)', shortLabel: 'LDN', zone: 'Europe/London' },
  { id: 'paris', label: 'Paris (CET/CEST)', shortLabel: 'PAR', zone: 'Europe/Paris' },
  { id: 'berlin', label: 'Berlin (CET/CEST)', shortLabel: 'BER', zone: 'Europe/Berlin' },
  { id: 'amsterdam', label: 'Amsterdam (CET/CEST)', shortLabel: 'AMS', zone: 'Europe/Amsterdam' },
  { id: 'zurich', label: 'Zurich (CET/CEST)', shortLabel: 'ZRH', zone: 'Europe/Zurich' },
  { id: 'moscow', label: 'Moscow (MSK)', shortLabel: 'MOW', zone: 'Europe/Moscow' },
  { id: 'istanbul', label: 'Istanbul (TRT)', shortLabel: 'IST', zone: 'Europe/Istanbul' },

  // Asia
  { id: 'dubai', label: 'Dubai (GST)', shortLabel: 'DXB', zone: 'Asia/Dubai' },
  { id: 'mumbai', label: 'Mumbai (IST)', shortLabel: 'BOM', zone: 'Asia/Kolkata' },
  { id: 'delhi', label: 'Delhi (IST)', shortLabel: 'DEL', zone: 'Asia/Kolkata' },
  { id: 'bangkok', label: 'Bangkok (ICT)', shortLabel: 'BKK', zone: 'Asia/Bangkok' },
  { id: 'singapore', label: 'Singapore (SGT)', shortLabel: 'SIN', zone: 'Asia/Singapore' },
  { id: 'hong-kong', label: 'Hong Kong (HKT)', shortLabel: 'HKG', zone: 'Asia/Hong_Kong' },
  { id: 'shanghai', label: 'Shanghai (CST)', shortLabel: 'SHA', zone: 'Asia/Shanghai' },
  { id: 'tokyo', label: 'Tokyo (JST)', shortLabel: 'TYO', zone: 'Asia/Tokyo' },
  { id: 'seoul', label: 'Seoul (KST)', shortLabel: 'SEL', zone: 'Asia/Seoul' },

  // Pacific & Oceania
  { id: 'sydney', label: 'Sydney (AEST/AEDT)', shortLabel: 'SYD', zone: 'Australia/Sydney' },
  { id: 'melbourne', label: 'Melbourne (AEST/AEDT)', shortLabel: 'MEL', zone: 'Australia/Melbourne' },
  { id: 'perth', label: 'Perth (AWST)', shortLabel: 'PER', zone: 'Australia/Perth' },
  { id: 'auckland', label: 'Auckland (NZST/NZDT)', shortLabel: 'AKL', zone: 'Pacific/Auckland' },

  // Africa & Middle East
  { id: 'johannesburg', label: 'Johannesburg (SAST)', shortLabel: 'JNB', zone: 'Africa/Johannesburg' },
  { id: 'cairo', label: 'Cairo (EET)', shortLabel: 'CAI', zone: 'Africa/Cairo' },
  { id: 'lagos', label: 'Lagos (WAT)', shortLabel: 'LOS', zone: 'Africa/Lagos' },
  { id: 'riyadh', label: 'Riyadh (AST)', shortLabel: 'RUH', zone: 'Asia/Riyadh' },
  { id: 'tel-aviv', label: 'Tel Aviv (IST)', shortLabel: 'TLV', zone: 'Asia/Jerusalem' },
];

const DEFAULT_STORAGE_KEY = 'fincept_default_timezone';
const SESSION_STORAGE_KEY = 'fincept_timezone';
const DEFAULT_TIMEZONE_ID = 'local';

// ============================================================================
// Context Definition
// ============================================================================

interface TimezoneContextValue {
  /** Default timezone (persisted, used by nav bar) */
  defaultTimezone: TimezoneOption;
  /** Session timezone (per-session, used by dashboard widgets) */
  timezone: TimezoneOption;
  /** All available timezone options */
  options: TimezoneOption[];
  /** Update the default timezone (persisted in settings) */
  setDefaultTimezone: (timezoneId: string) => void;
  /** Update the session timezone (for dashboard widgets) */
  setTimezone: (timezoneId: string) => void;
  /** Format a Date object according to a specific timezone */
  formatTime: (date: Date, options?: Intl.DateTimeFormatOptions) => string;
  /** Format a Date object according to the default timezone */
  formatTimeWithDefault: (date: Date, options?: Intl.DateTimeFormatOptions) => string;
  /** Format a Date object to a short time string (HH:MM:SS) */
  formatTimeShort: (date: Date) => string;
  /** Get the current time formatted in the selected timezone */
  getCurrentTimeFormatted: () => string;
}

const TimezoneContext = createContext<TimezoneContextValue | undefined>(undefined);

// ============================================================================
// Provider Component
// ============================================================================

interface TimezoneProviderProps {
  children: ReactNode;
}

export const TimezoneProvider: React.FC<TimezoneProviderProps> = ({ children }) => {
  // Initialize default timezone
  const [defaultTimezoneId, setDefaultTimezoneId] = useState<string>(DEFAULT_TIMEZONE_ID);

  // Initialize session timezone
  const [sessionTimezoneId, setSessionTimezoneId] = useState<string>(DEFAULT_TIMEZONE_ID);

  // Get the full timezone option objects
  const defaultTimezone = TIMEZONE_OPTIONS.find(tz => tz.id === defaultTimezoneId) || TIMEZONE_OPTIONS[0];
  const timezone = TIMEZONE_OPTIONS.find(tz => tz.id === sessionTimezoneId) || TIMEZONE_OPTIONS[0];

  // Load timezones from storage on mount
  useEffect(() => {
    const loadTimezones = async () => {
      try {
        const storedDefault = await getSetting(DEFAULT_STORAGE_KEY);
        if (storedDefault && TIMEZONE_OPTIONS.find(tz => tz.id === storedDefault)) {
          setDefaultTimezoneId(storedDefault);
        }

        const storedSession = await getSetting(SESSION_STORAGE_KEY);
        if (storedSession && TIMEZONE_OPTIONS.find(tz => tz.id === storedSession)) {
          setSessionTimezoneId(storedSession);
        } else if (storedDefault && TIMEZONE_OPTIONS.find(tz => tz.id === storedDefault)) {
          setSessionTimezoneId(storedDefault);
        }
      } catch (e) {
        console.warn('[TimezoneContext] Failed to load timezones from storage:', e);
      }
    };
    loadTimezones();
  }, []);

  // Persist default timezone to storage when it changes
  useEffect(() => {
    const saveDefaultTimezone = async () => {
      try {
        await saveSetting(DEFAULT_STORAGE_KEY, defaultTimezoneId, 'timezone');
      } catch (e) {
        console.warn('[TimezoneContext] Failed to save default timezone:', e);
      }
    };
    saveDefaultTimezone();
  }, [defaultTimezoneId]);

  // Persist session timezone to storage when it changes
  useEffect(() => {
    const saveSessionTimezone = async () => {
      try {
        await saveSetting(SESSION_STORAGE_KEY, sessionTimezoneId, 'timezone');
      } catch (e) {
        console.warn('[TimezoneContext] Failed to save session timezone:', e);
      }
    };
    saveSessionTimezone();
  }, [sessionTimezoneId]);

  // Set default timezone by ID (used in Settings)
  const setDefaultTimezone = useCallback((id: string) => {
    if (TIMEZONE_OPTIONS.find(tz => tz.id === id)) {
      setDefaultTimezoneId(id);
    } else {
      console.warn(`[TimezoneContext] Unknown timezone ID: ${id}`);
    }
  }, []);

  // Set session timezone by ID (used in dashboard widgets)
  const setTimezone = useCallback((id: string) => {
    if (TIMEZONE_OPTIONS.find(tz => tz.id === id)) {
      setSessionTimezoneId(id);
    } else {
      console.warn(`[TimezoneContext] Unknown timezone ID: ${id}`);
    }
  }, []);

  // Format a date according to the session timezone
  const formatTime = useCallback((date: Date, options?: Intl.DateTimeFormatOptions): string => {
    const defaultOptions: Intl.DateTimeFormatOptions = {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false,
      ...options,
    };

    // Add timezone if not using local
    if (timezone.zone) {
      defaultOptions.timeZone = timezone.zone;
    }

    try {
      return new Intl.DateTimeFormat('en-US', defaultOptions).format(date);
    } catch (e) {
      console.error('[TimezoneContext] Format error:', e);
      return date.toLocaleTimeString('en-US', { hour12: false });
    }
  }, [timezone]);

  // Format a date according to the DEFAULT timezone (for nav bar)
  const formatTimeWithDefault = useCallback((date: Date, options?: Intl.DateTimeFormatOptions): string => {
    const defaultOptions: Intl.DateTimeFormatOptions = {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false,
      ...options,
    };

    // Add timezone if not using local
    if (defaultTimezone.zone) {
      defaultOptions.timeZone = defaultTimezone.zone;
    }

    try {
      return new Intl.DateTimeFormat('en-US', defaultOptions).format(date);
    } catch (e) {
      console.error('[TimezoneContext] Format error:', e);
      return date.toLocaleTimeString('en-US', { hour12: false });
    }
  }, [defaultTimezone]);

  // Short format: HH:MM:SS
  const formatTimeShort = useCallback((date: Date): string => {
    return formatTime(date, {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false,
    });
  }, [formatTime]);

  // Get current time formatted
  const getCurrentTimeFormatted = useCallback((): string => {
    return formatTimeShort(new Date());
  }, [formatTimeShort]);

  const value: TimezoneContextValue = {
    defaultTimezone,
    timezone,
    options: TIMEZONE_OPTIONS,
    setDefaultTimezone,
    setTimezone,
    formatTime,
    formatTimeWithDefault,
    formatTimeShort,
    getCurrentTimeFormatted,
  };

  return (
    <TimezoneContext.Provider value={value}>
      {children}
    </TimezoneContext.Provider>
  );
};

// ============================================================================
// Hook
// ============================================================================

/**
 * Hook to access timezone context.
 * Must be used within a TimezoneProvider.
 */
export const useTimezone = (): TimezoneContextValue => {
  const context = useContext(TimezoneContext);
  if (!context) {
    throw new Error('useTimezone must be used within a TimezoneProvider');
  }
  return context;
};

// ============================================================================
// Utility Hook: useCurrentTime (for dashboard widgets)
// ============================================================================

/**
 * Hook that provides a ticking current time, automatically formatted
 * according to the SESSION timezone (for dashboard widgets).
 * 
 * @param refreshInterval - Update interval in milliseconds (default: 1000)
 * @returns Object with currentTime (Date) and formattedTime (string)
 */
export const useCurrentTime = (refreshInterval: number = 1000) => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const { formatTimeShort, timezone } = useTimezone();

  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, refreshInterval);

    return () => clearInterval(timer);
  }, [refreshInterval]);

  return {
    currentTime,
    formattedTime: formatTimeShort(currentTime),
    timezone,
  };
};

// ============================================================================
// Utility Hook: useDefaultTime (for navigation bar)
// ============================================================================

/**
 * Hook that provides a ticking current time, automatically formatted
 * according to the DEFAULT timezone (for navigation bar).
 * 
 * @param refreshInterval - Update interval in milliseconds (default: 1000)
 * @returns Object with currentTime (Date), formattedTime (string), and timezone
 */
export const useDefaultTime = (refreshInterval: number = 1000) => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const { formatTimeWithDefault, defaultTimezone } = useTimezone();

  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, refreshInterval);

    return () => clearInterval(timer);
  }, [refreshInterval]);

  return {
    currentTime,
    formattedTime: formatTimeWithDefault(currentTime),
    timezone: defaultTimezone,
  };
};

export default TimezoneContext;

