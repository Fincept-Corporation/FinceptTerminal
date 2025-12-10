// CRITICAL: Must be first import to set up Buffer/process globals
import './polyfills/init'

import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App'
import { AuthProvider } from './contexts/AuthContext'
import { LanguageProvider } from './contexts/LanguageContext'
import { BrokerProvider } from './contexts/BrokerContext'
import './i18n/config' // Initialize i18n
import './App.css' // Make sure this line exists
import './tauri-cors-config' // Initialize Tauri CORS plugin

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
            <App />
          </BrokerProvider>
        </LanguageProvider>
      </AuthProvider>
    </ErrorBoundary>
  </React.StrictMode>,
)