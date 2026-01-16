/**
 * Logger Service
 * Provides structured logging with levels, namespaces, and environment-based filtering.
 * Replaces scattered console.log statements throughout the codebase.
 */

export type LogLevel = 'debug' | 'info' | 'warn' | 'error';

interface LogEntry {
    timestamp: string;
    level: LogLevel;
    namespace: string;
    message: string;
    data?: unknown;
}

interface LoggerOptions {
    namespace: string;
    enabled?: boolean;
}

// Log levels in order of severity
const LOG_LEVELS: Record<LogLevel, number> = {
    debug: 0,
    info: 1,
    warn: 2,
    error: 3,
};

// Environment-based minimum log level
const getMinLogLevel = (): LogLevel => {
    // In production, only show warnings and errors
    // In development, show all logs
    const isDev = import.meta.env?.DEV ?? process.env.NODE_ENV === 'development';
    return isDev ? 'debug' : 'warn';
};

// Global log history for debugging
const logHistory: LogEntry[] = [];
const MAX_HISTORY_SIZE = 1000;

/**
 * Creates a namespaced logger instance
 */
export function createLogger(options: LoggerOptions) {
    const { namespace, enabled = true } = options;
    const minLevel = getMinLogLevel();

    const shouldLog = (level: LogLevel): boolean => {
        if (!enabled) return false;
        return LOG_LEVELS[level] >= LOG_LEVELS[minLevel];
    };

    const formatMessage = (level: LogLevel, message: string, data?: unknown): LogEntry => {
        return {
            timestamp: new Date().toISOString(),
            level,
            namespace,
            message,
            data,
        };
    };

    const logToConsole = (entry: LogEntry): void => {
        // Disabled for production - logs only stored in history
        // Only log errors
        if (entry.level === 'error') {
            const prefix = `[${entry.namespace}]`;
            const args = entry.data !== undefined ? [prefix, entry.message, entry.data] : [prefix, entry.message];
            console.error(...args);
        }
    };

    const addToHistory = (entry: LogEntry): void => {
        logHistory.push(entry);
        if (logHistory.length > MAX_HISTORY_SIZE) {
            logHistory.shift();
        }
    };

    const log = (level: LogLevel, message: string, data?: unknown): void => {
        const entry = formatMessage(level, message, data);
        addToHistory(entry);

        if (shouldLog(level)) {
            logToConsole(entry);
        }
    };

    return {
        debug: (message: string, data?: unknown) => log('debug', message, data),
        info: (message: string, data?: unknown) => log('info', message, data),
        warn: (message: string, data?: unknown) => log('warn', message, data),
        error: (message: string, data?: unknown) => log('error', message, data),

        /** Get the namespace of this logger */
        getNamespace: () => namespace,

        /** Check if logging is enabled */
        isEnabled: () => enabled,
    };
}

/**
 * Global logger utilities
 */
export const LoggerUtils = {
    /** Get all log entries from history */
    getHistory: (): LogEntry[] => [...logHistory],

    /** Get log entries filtered by level */
    getHistoryByLevel: (level: LogLevel): LogEntry[] =>
        logHistory.filter((entry) => entry.level === level),

    /** Get log entries filtered by namespace */
    getHistoryByNamespace: (namespace: string): LogEntry[] =>
        logHistory.filter((entry) => entry.namespace === namespace),

    /** Clear log history */
    clearHistory: (): void => {
        logHistory.length = 0;
    },

    /** Export logs as JSON for debugging */
    exportAsJson: (): string => JSON.stringify(logHistory, null, 2),
};

// Pre-configured loggers for common services
export const workflowLogger = createLogger({ namespace: 'WorkflowService' });
export const websocketLogger = createLogger({ namespace: 'WebSocket' });
export const mcpLogger = createLogger({ namespace: 'MCP' });
export const authLogger = createLogger({ namespace: 'Auth' });
export const sqliteLogger = createLogger({ namespace: 'SQLite' });
export const watchlistLogger = createLogger({ namespace: 'Watchlist' });
export const portfolioLogger = createLogger({ namespace: 'Portfolio' });
export const llmLogger = createLogger({ namespace: 'LLM' });
export const nodeLogger = createLogger({ namespace: 'NodeExecution' });
export const polygonLogger = createLogger({ namespace: 'Polygon' });
export const pythonAgentLogger = createLogger({ namespace: 'PythonAgent' });
export const marketDataLogger = createLogger({ namespace: 'MarketData' });
export const fyersLogger = createLogger({ namespace: 'Fyers' });
export const backtestingLogger = createLogger({ namespace: 'Backtesting' });
export const contextLogger = createLogger({ namespace: 'ContextRecorder' });
export const agentLogger = createLogger({ namespace: 'AgentLLM' });
export const geopoliticsLogger = createLogger({ namespace: 'Geopolitics' });
export const reportLogger = createLogger({ namespace: 'Report' });
export const notebookLogger = createLogger({ namespace: 'Notebook' });
export const dataSourceLogger = createLogger({ namespace: 'DataSource' });




