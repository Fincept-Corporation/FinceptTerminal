// CRITICAL: Must be first import to set up Buffer/process globals
import './polyfills/init'

// Suppress development warnings in console
if (process.env.NODE_ENV === 'production') {
  const originalWarn = console.warn;
  console.warn = (...args) => {
    const message = args[0]?.toString() || '';
    // Suppress React DevTools and Three.js warnings
    if (message.includes('Download the React DevTools') ||
        message.includes('three.js') ||
        message.includes('three.min.js') ||
        message.includes('Multiple instances of Three.js')) {
      return;
    }
    originalWarn.apply(console, args);
  };
}

import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App'
import { AuthProvider } from './contexts/AuthContext'
import { LanguageProvider } from './contexts/LanguageContext'
import { BrokerProvider } from './contexts/BrokerContext'
import { TimezoneProvider } from './contexts/TimezoneContext'
import './i18n/config' // Initialize i18n
import './App.css' // Make sure this line exists
import './tauri-cors-config' // Initialize Tauri CORS plugin

// Initialize backtesting providers
import { backtestingRegistry } from './services/backtesting/BacktestingProviderRegistry'
import { LeanAdapter } from './services/backtesting/adapters/lean'
import { VectorBTAdapter } from './services/backtesting/adapters/vectorbt/VectorBTAdapter'
import { BacktestingPyAdapter } from './services/backtesting/adapters/backtestingpy'
import { FastTradeAdapter } from './services/backtesting/adapters/fasttrade'

// Register default backtesting providers
try {
  const leanAdapter = new LeanAdapter();
  backtestingRegistry.registerProvider(leanAdapter);

  const vectorbtAdapter = new VectorBTAdapter();
  backtestingRegistry.registerProvider(vectorbtAdapter);

  const backtestingPyAdapter = new BacktestingPyAdapter();
  backtestingRegistry.registerProvider(backtestingPyAdapter);

  const fastTradeAdapter = new FastTradeAdapter();
  backtestingRegistry.registerProvider(fastTradeAdapter);

} catch (error) {
  console.error('[App] Failed to register backtesting providers:', error);
}

// Error Boundary Component
class ErrorBoundary extends React.Component<
  { children: React.ReactNode },
  { hasError: boolean; error: Error | null }
> {
  constructor(props: { children: React.ReactNode }) {
    super(props);
    this.state = { hasError: false, error: null };
  }

  static getDerivedStateFromError(error: Error) {
    return { hasError: true, error };
  }

  componentDidCatch(error: Error, errorInfo: React.ErrorInfo) {
    console.error('Error caught by boundary:', error, errorInfo);
  }

  render() {
    if (this.state.hasError) {
      return (
        <div style={{ padding: '20px', color: 'white', backgroundColor: '#1a1a1a' }}>
          <h1>Something went wrong</h1>
          <pre style={{ color: '#ff6b6b' }}>{this.state.error?.toString()}</pre>
          <pre style={{ fontSize: '12px', marginTop: '10px' }}>
            {this.state.error?.stack}
          </pre>
        </div>
      );
    }

    return this.props.children;
  }
}

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <ErrorBoundary>
      <AuthProvider>
        <LanguageProvider>
          <BrokerProvider>
            <TimezoneProvider>
              <App />
            </TimezoneProvider>
          </BrokerProvider>
        </LanguageProvider>
      </AuthProvider>
    </ErrorBoundary>
  </React.StrictMode>,
)