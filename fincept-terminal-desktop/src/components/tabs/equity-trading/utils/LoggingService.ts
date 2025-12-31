/**
 * Logging Service
 * Structured logging for trading operations
 */

export enum LogLevel {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
}

export interface LogEntry {
  timestamp: Date;
  level: LogLevel;
  category: string;
  message: string;
  data?: any;
  brokerId?: string;
}

export class LoggingService {
  private static instance: LoggingService;
  private logs: LogEntry[] = [];
  private maxLogs = 1000;
  private minLevel: LogLevel = LogLevel.INFO;
  private listeners: Set<(entry: LogEntry) => void> = new Set();

  private constructor() {}

  static getInstance(): LoggingService {
    if (!LoggingService.instance) {
      LoggingService.instance = new LoggingService();
    }
    return LoggingService.instance;
  }

  /**
   * Set minimum log level
   */
  setMinLevel(level: LogLevel): void {
    this.minLevel = level;
  }

  /**
   * Subscribe to log events
   */
  subscribe(listener: (entry: LogEntry) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  /**
   * Log debug message
   */
  debug(category: string, message: string, data?: any, brokerId?: string): void {
    this.log(LogLevel.DEBUG, category, message, data, brokerId);
  }

  /**
   * Log info message
   */
  info(category: string, message: string, data?: any, brokerId?: string): void {
    this.log(LogLevel.INFO, category, message, data, brokerId);
  }

  /**
   * Log warning
   */
  warn(category: string, message: string, data?: any, brokerId?: string): void {
    this.log(LogLevel.WARN, category, message, data, brokerId);
  }

  /**
   * Log error
   */
  error(category: string, message: string, data?: any, brokerId?: string): void {
    this.log(LogLevel.ERROR, category, message, data, brokerId);
  }

  /**
   * Core logging method
   */
  private log(
    level: LogLevel,
    category: string,
    message: string,
    data?: any,
    brokerId?: string
  ): void {
    if (level < this.minLevel) return;

    const entry: LogEntry = {
      timestamp: new Date(),
      level,
      category,
      message,
      data,
      brokerId,
    };

    // Add to logs array
    this.logs.push(entry);

    // Trim logs if exceeding max
    if (this.logs.length > this.maxLogs) {
      this.logs.shift();
    }

    // Console output
    const levelName = LogLevel[level];
    const prefix = `[${entry.timestamp.toISOString()}] [${levelName}] [${category}]`;
    const brokerPrefix = brokerId ? ` [${brokerId.toUpperCase()}]` : '';

    switch (level) {
      case LogLevel.DEBUG:
        console.debug(`${prefix}${brokerPrefix}`, message, data || '');
        break;
      case LogLevel.INFO:
        console.info(`${prefix}${brokerPrefix}`, message, data || '');
        break;
      case LogLevel.WARN:
        console.warn(`${prefix}${brokerPrefix}`, message, data || '');
        break;
      case LogLevel.ERROR:
        console.error(`${prefix}${brokerPrefix}`, message, data || '');
        break;
    }

    // Notify listeners
    this.listeners.forEach((listener) => {
      try {
        listener(entry);
      } catch (error) {
        console.error('[LoggingService] Listener error:', error);
      }
    });
  }

  /**
   * Get all logs
   */
  getLogs(): LogEntry[] {
    return [...this.logs];
  }

  /**
   * Get logs by category
   */
  getLogsByCategory(category: string): LogEntry[] {
    return this.logs.filter((log) => log.category === category);
  }

  /**
   * Get logs by broker
   */
  getLogsByBroker(brokerId: string): LogEntry[] {
    return this.logs.filter((log) => log.brokerId === brokerId);
  }

  /**
   * Get logs by level
   */
  getLogsByLevel(level: LogLevel): LogEntry[] {
    return this.logs.filter((log) => log.level === level);
  }

  /**
   * Clear all logs
   */
  clearLogs(): void {
    this.logs = [];
    console.log('[LoggingService] Logs cleared');
  }

  /**
   * Export logs to JSON
   */
  exportLogs(): string {
    return JSON.stringify(this.logs, null, 2);
  }

  /**
   * Get statistics
   */
  getStats(): {
    total: number;
    byLevel: Record<string, number>;
    byCategory: Record<string, number>;
  } {
    const byLevel: Record<string, number> = {};
    const byCategory: Record<string, number> = {};

    this.logs.forEach((log) => {
      const levelName = LogLevel[log.level];
      byLevel[levelName] = (byLevel[levelName] || 0) + 1;
      byCategory[log.category] = (byCategory[log.category] || 0) + 1;
    });

    return {
      total: this.logs.length,
      byLevel,
      byCategory,
    };
  }
}

export const loggingService = LoggingService.getInstance();

// Convenience exports
export const logger = {
  debug: (category: string, message: string, data?: any, brokerId?: string) =>
    loggingService.debug(category, message, data, brokerId),
  info: (category: string, message: string, data?: any, brokerId?: string) =>
    loggingService.info(category, message, data, brokerId),
  warn: (category: string, message: string, data?: any, brokerId?: string) =>
    loggingService.warn(category, message, data, brokerId),
  error: (category: string, message: string, data?: any, brokerId?: string) =>
    loggingService.error(category, message, data, brokerId),
};
