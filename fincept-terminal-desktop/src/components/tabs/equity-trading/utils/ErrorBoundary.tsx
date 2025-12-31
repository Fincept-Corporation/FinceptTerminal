/**
 * Error Boundary
 * Catch and handle React errors gracefully
 */

import React, { Component, ErrorInfo, ReactNode } from 'react';

interface Props {
  children: ReactNode;
  fallback?: ReactNode;
  onError?: (error: Error, errorInfo: ErrorInfo) => void;
}

interface State {
  hasError: boolean;
  error: Error | null;
}

export class ErrorBoundary extends Component<Props, State> {
  constructor(props: Props) {
    super(props);
    this.state = {
      hasError: false,
      error: null,
    };
  }

  static getDerivedStateFromError(error: Error): State {
    return {
      hasError: true,
      error,
    };
  }

  componentDidCatch(error: Error, errorInfo: ErrorInfo): void {
    console.error('[ErrorBoundary] Caught error:', error, errorInfo);

    // Call optional error handler
    this.props.onError?.(error, errorInfo);
  }

  handleReset = (): void => {
    this.setState({
      hasError: false,
      error: null,
    });
  };

  render(): ReactNode {
    if (this.state.hasError) {
      // Custom fallback UI
      if (this.props.fallback) {
        return this.props.fallback;
      }

      // Default error UI
      return (
        <div className="min-h-[400px] flex items-center justify-center p-6">
          <div className="max-w-md w-full bg-[#111] border border-red-500/30 rounded-lg p-6">
            <div className="flex items-start gap-4">
              <div className="text-3xl">⚠️</div>
              <div className="flex-1">
                <h3 className="text-lg font-semibold text-red-500 mb-2">
                  Something went wrong
                </h3>
                <p className="text-sm text-muted-foreground mb-4">
                  An error occurred while rendering this component. Please try refreshing or contact support if the issue persists.
                </p>

                {this.state.error && (
                  <details className="mb-4 text-xs">
                    <summary className="cursor-pointer text-muted-foreground hover:text-white">
                      Error details
                    </summary>
                    <pre className="mt-2 p-2 bg-[#0a0a0a] border border-border rounded overflow-auto max-h-40">
                      {this.state.error.toString()}
                    </pre>
                  </details>
                )}

                <button
                  onClick={this.handleReset}
                  className="px-4 py-2 bg-primary text-primary-foreground rounded hover:bg-primary/90 text-sm"
                >
                  Try Again
                </button>
              </div>
            </div>
          </div>
        </div>
      );
    }

    return this.props.children;
  }
}

export default ErrorBoundary;
