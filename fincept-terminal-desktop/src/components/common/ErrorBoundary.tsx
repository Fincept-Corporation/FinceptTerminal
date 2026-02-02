/**
 * Generic Error Boundary - Catches runtime errors in React components
 *
 * Features:
 * - Prevents cascading failures (one component crash doesn't kill the page)
 * - Customizable fallback UI
 * - Error logging with component context
 * - Retry functionality
 *
 * Usage:
 *   <ErrorBoundary name="MarketPanel" onRetry={() => refetch()}>
 *     <MyComponent />
 *   </ErrorBoundary>
 */

import React, { Component, ErrorInfo, ReactNode } from 'react';

// ============================================================================
// Types
// ============================================================================

export interface ErrorBoundaryProps {
  /** Child components to wrap */
  children: ReactNode;
  /** Name for error logging (e.g., "Stock Indices Panel") */
  name?: string;
  /** Custom fallback UI */
  fallback?: ReactNode | ((error: Error, reset: () => void) => ReactNode);
  /** Callback when error is caught */
  onError?: (error: Error, errorInfo: ErrorInfo) => void;
  /** Callback for retry button */
  onRetry?: () => void;
  /** Custom styling */
  className?: string;
  /** Variant for different visual styles */
  variant?: 'default' | 'minimal' | 'inline';
}

interface ErrorBoundaryState {
  hasError: boolean;
  error: Error | null;
  errorInfo: ErrorInfo | null;
}

// ============================================================================
// Component
// ============================================================================

export class ErrorBoundary extends Component<ErrorBoundaryProps, ErrorBoundaryState> {
  constructor(props: ErrorBoundaryProps) {
    super(props);
    this.state = {
      hasError: false,
      error: null,
      errorInfo: null,
    };
  }

  static getDerivedStateFromError(error: Error): Partial<ErrorBoundaryState> {
    return { hasError: true, error };
  }

  componentDidCatch(error: Error, errorInfo: ErrorInfo): void {
    this.setState({ errorInfo });

    const { name, onError } = this.props;

    // Log error with context
    console.error(`[ErrorBoundary${name ? `: ${name}` : ''}] Caught error:`, {
      error: error.message,
      stack: error.stack,
      componentStack: errorInfo.componentStack,
    });

    // Call custom error handler
    onError?.(error, errorInfo);

    // TODO: Send to monitoring service (Sentry, etc.)
    // if (typeof window !== 'undefined' && window.Sentry) {
    //   window.Sentry.captureException(error, { extra: { componentStack: errorInfo.componentStack } });
    // }
  }

  handleReset = (): void => {
    this.setState({ hasError: false, error: null, errorInfo: null });
    this.props.onRetry?.();
  };

  render(): ReactNode {
    const { hasError, error } = this.state;
    const { children, name, fallback, variant = 'default', className } = this.props;

    if (!hasError) {
      return children;
    }

    // Custom fallback function
    if (typeof fallback === 'function') {
      return fallback(error!, this.handleReset);
    }

    // Custom fallback element
    if (fallback) {
      return fallback;
    }

    // Default fallback based on variant
    switch (variant) {
      case 'minimal':
        return (
          <div className={className} style={styles.minimal.container}>
            <span style={styles.minimal.text}>Error loading {name || 'component'}</span>
            <button onClick={this.handleReset} style={styles.minimal.button}>
              Retry
            </button>
          </div>
        );

      case 'inline':
        return (
          <span className={className} style={styles.inline.container}>
            <span style={styles.inline.icon}>⚠</span>
            <span style={styles.inline.text}>Error</span>
            <button onClick={this.handleReset} style={styles.inline.button}>
              ↻
            </button>
          </span>
        );

      default:
        return (
          <div className={className} style={styles.default.container}>
            <div style={styles.default.icon}>⚠</div>
            <div style={styles.default.title}>
              {name ? `${name} Error` : 'Something went wrong'}
            </div>
            <div style={styles.default.message}>
              {error?.message || 'An unexpected error occurred'}
            </div>
            <button onClick={this.handleReset} style={styles.default.button}>
              Try Again
            </button>
            <div style={styles.default.hint}>
              If this persists, try refreshing the page
            </div>
          </div>
        );
    }
  }
}

// ============================================================================
// Styles (inline to avoid CSS dependencies)
// ============================================================================

const styles = {
  default: {
    container: {
      display: 'flex',
      flexDirection: 'column' as const,
      alignItems: 'center',
      justifyContent: 'center',
      padding: '24px',
      backgroundColor: '#1a1a1a',
      border: '1px solid #ef4444',
      borderRadius: '4px',
      textAlign: 'center' as const,
      minHeight: '200px',
      gap: '12px',
    },
    icon: {
      fontSize: '32px',
      color: '#ef4444',
    },
    title: {
      fontSize: '14px',
      fontWeight: 'bold' as const,
      color: '#ef4444',
    },
    message: {
      fontSize: '12px',
      color: '#a3a3a3',
      maxWidth: '300px',
    },
    button: {
      backgroundColor: '#ea580c',
      color: '#000',
      border: 'none',
      padding: '8px 20px',
      fontSize: '12px',
      fontWeight: 'bold' as const,
      cursor: 'pointer',
      borderRadius: '4px',
    },
    hint: {
      fontSize: '10px',
      color: '#525252',
    },
  },
  minimal: {
    container: {
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      gap: '8px',
      padding: '8px 12px',
      backgroundColor: 'rgba(239, 68, 68, 0.1)',
      borderRadius: '4px',
    },
    text: {
      fontSize: '12px',
      color: '#ef4444',
    },
    button: {
      backgroundColor: 'transparent',
      color: '#ef4444',
      border: '1px solid #ef4444',
      padding: '2px 8px',
      fontSize: '11px',
      cursor: 'pointer',
      borderRadius: '2px',
    },
  },
  inline: {
    container: {
      display: 'inline-flex',
      alignItems: 'center',
      gap: '4px',
      padding: '2px 6px',
      backgroundColor: 'rgba(239, 68, 68, 0.1)',
      borderRadius: '2px',
    },
    icon: {
      fontSize: '12px',
      color: '#ef4444',
    },
    text: {
      fontSize: '11px',
      color: '#ef4444',
    },
    button: {
      backgroundColor: 'transparent',
      color: '#ef4444',
      border: 'none',
      padding: '0 2px',
      fontSize: '12px',
      cursor: 'pointer',
    },
  },
};

// ============================================================================
// HOC for wrapping components
// ============================================================================

export function withErrorBoundary<P extends object>(
  WrappedComponent: React.ComponentType<P>,
  boundaryProps?: Omit<ErrorBoundaryProps, 'children'>
) {
  const displayName = WrappedComponent.displayName || WrappedComponent.name || 'Component';

  const WithErrorBoundary: React.FC<P> = (props) => (
    <ErrorBoundary name={displayName} {...boundaryProps}>
      <WrappedComponent {...props} />
    </ErrorBoundary>
  );

  WithErrorBoundary.displayName = `withErrorBoundary(${displayName})`;

  return WithErrorBoundary;
}

export default ErrorBoundary;
