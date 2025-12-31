// SetupScreen.tsx - First-time setup screen with progress tracking
import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';

interface SetupProgress {
  step: string;
  progress: number;
  message: string;
  is_error: boolean;
}

interface SetupStatus {
  python_installed: boolean;
  bun_installed: boolean;
  python_version: string | null;
  bun_version: string | null;
  needs_setup: boolean;
}

interface SetupScreenProps {
  onSetupComplete: () => void;
}

const SetupScreen: React.FC<SetupScreenProps> = ({ onSetupComplete }) => {
  const [isChecking, setIsChecking] = useState(true);
  const [isInstalling, setIsInstalling] = useState(false);
  const [setupStatus, setSetupStatus] = useState<SetupStatus | null>(null);
  const [pythonProgress, setPythonProgress] = useState(0);
  const [bunProgress, setBunProgress] = useState(0);
  const [packagesProgress, setPackagesProgress] = useState(0);
  const [currentStep, setCurrentStep] = useState('');
  const [currentMessage, setCurrentMessage] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [installationLogs, setInstallationLogs] = useState<string[]>([]);
  const [showLogs, setShowLogs] = useState(false);

  useEffect(() => {
    checkSetupStatus();
    setupProgressListener();
  }, []);

  const checkSetupStatus = async () => {
    try {
      setIsChecking(true);
      const status: SetupStatus = await invoke('check_setup_status');
      setSetupStatus(status);

      if (!status.needs_setup) {
        // Already set up, proceed to app
        setTimeout(() => onSetupComplete(), 1000);
      } else {
        // Need to run setup
        setIsChecking(false);
        runSetup();
      }
    } catch (err) {
      console.error('Failed to check setup status:', err);
      setError(String(err));
      setIsChecking(false);
    }
  };

  const setupProgressListener = async () => {
    const unlisten = await listen<SetupProgress>('setup-progress', (event) => {
      const progress = event.payload;
      setCurrentStep(progress.step);
      setCurrentMessage(progress.message);

      // Add to logs for transparency
      if (progress.message) {
        setInstallationLogs(prev => [...prev, `[${progress.step}] ${progress.message}`]);
      }

      if (progress.is_error) {
        setError(progress.message);
        return;
      }

      // Update progress for each step
      switch (progress.step) {
        case 'python':
          setPythonProgress(progress.progress);
          break;
        case 'bun':
          setBunProgress(progress.progress);
          break;
        case 'packages':
          setPackagesProgress(progress.progress);
          break;
        case 'complete':
          setTimeout(() => onSetupComplete(), 2000);
          break;
      }
    });

    // Cleanup listener on unmount
    return () => {
      unlisten();
    };
  };

  const runSetup = async () => {
    try {
      setIsInstalling(true);
      setError(null);
      await invoke('run_setup');
    } catch (err) {
      console.error('Setup failed:', err);
      setError(String(err));
      setIsInstalling(false);
    }
  };

  const retrySetup = () => {
    console.log('[SetupScreen] Retry button clicked');
    setError(null);
    setPythonProgress(0);
    setBunProgress(0);
    setPackagesProgress(0);
    setCurrentStep('');
    setCurrentMessage('');
    setInstallationLogs([]);
    setShowLogs(false);
    setIsInstalling(true);
    runSetup();
  };

  if (isChecking) {
    return (
      <div className="min-h-screen bg-black flex items-center justify-center">
        <div className="text-center space-y-6">
          <div className="relative">
            <div className="w-20 h-20 border-4 border-zinc-800 rounded-full mx-auto"></div>
            <div className="w-20 h-20 border-4 border-t-emerald-500 rounded-full animate-spin absolute top-0 left-1/2 transform -translate-x-1/2"></div>
          </div>
          <div>
            <h2 className="text-2xl font-bold text-white mb-2">Fincept Terminal</h2>
            <p className="text-zinc-400">Checking system requirements...</p>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-black flex items-center justify-center p-8">
      <div className="max-w-2xl w-full space-y-8">
        {/* Header */}
        <div className="text-center space-y-4">
          <h1 className="text-4xl font-bold text-white">Welcome to Fincept Terminal</h1>
          <p className="text-zinc-400 text-lg">
            Setting up your environment for the first time...
          </p>
          {setupStatus && !setupStatus.needs_setup && (
            <p className="text-emerald-500 text-sm">[OK] System is ready!</p>
          )}
        </div>

        {/* Setup Progress */}
        {(isInstalling || setupStatus?.needs_setup) && (
          <div className="bg-zinc-900 rounded-lg border border-zinc-800 p-8 space-y-6">
            {/* Python Installation */}
            <div className="space-y-3">
              <div className="flex items-center justify-between">
                <div className="flex items-center space-x-3">
                  <div className={`w-8 h-8 rounded-full flex items-center justify-center transition-all duration-300 ${
                    pythonProgress === 100 ? 'bg-emerald-500 scale-110' :
                    pythonProgress > 0 ? 'bg-blue-500 animate-pulse' : 'bg-zinc-800'
                  }`}>
                    {pythonProgress === 100 ? '[OK]' : '1'}
                  </div>
                  <div className="flex-1">
                    <h3 className="text-white font-semibold">Python 3.12.7</h3>
                    <p className="text-zinc-500 text-sm">
                      {setupStatus?.python_installed
                        ? `Installed: ${setupStatus.python_version}`
                        : 'Required for analytics and data processing'}
                    </p>
                  </div>
                </div>
                <span className="text-zinc-400 text-sm font-mono">{pythonProgress}%</span>
              </div>
              <div className="w-full bg-zinc-800 rounded-full h-2 overflow-hidden">
                <div
                  className="bg-gradient-to-r from-blue-500 to-emerald-500 h-full transition-all duration-500 ease-out"
                  style={{ width: `${pythonProgress}%` }}
                />
              </div>
              {currentStep === 'python' && currentMessage && (
                <div className="flex items-center space-x-2 text-sm">
                  <div className="w-1.5 h-1.5 bg-blue-500 rounded-full animate-pulse"></div>
                  <p className="text-blue-300 font-medium animate-fade-in">{currentMessage}</p>
                </div>
              )}
            </div>

            {/* Bun Installation */}
            <div className="space-y-3">
              <div className="flex items-center justify-between">
                <div className="flex items-center space-x-3">
                  <div className={`w-8 h-8 rounded-full flex items-center justify-center transition-all duration-300 ${
                    bunProgress === 100 ? 'bg-emerald-500 scale-110' :
                    bunProgress > 0 ? 'bg-blue-500 animate-pulse' : 'bg-zinc-800'
                  }`}>
                    {bunProgress === 100 ? '[OK]' : '2'}
                  </div>
                  <div className="flex-1">
                    <h3 className="text-white font-semibold">Bun v1.1.0</h3>
                    <p className="text-zinc-500 text-sm">
                      {setupStatus?.bun_installed
                        ? `Installed: ${setupStatus.bun_version}`
                        : 'Required for MCP servers (bunx)'}
                    </p>
                  </div>
                </div>
                <span className="text-zinc-400 text-sm font-mono">{bunProgress}%</span>
              </div>
              <div className="w-full bg-zinc-800 rounded-full h-2 overflow-hidden">
                <div
                  className="bg-gradient-to-r from-blue-500 to-emerald-500 h-full transition-all duration-500 ease-out"
                  style={{ width: `${bunProgress}%` }}
                />
              </div>
              {currentStep === 'bun' && currentMessage && (
                <div className="flex items-center space-x-2 text-sm">
                  <div className="w-1.5 h-1.5 bg-blue-500 rounded-full animate-pulse"></div>
                  <p className="text-blue-300 font-medium animate-fade-in">{currentMessage}</p>
                </div>
              )}
            </div>

            {/* Python Packages Installation */}
            <div className="space-y-3">
              <div className="flex items-center justify-between">
                <div className="flex items-center space-x-3">
                  <div className={`w-8 h-8 rounded-full flex items-center justify-center transition-all duration-300 ${
                    packagesProgress === 100 ? 'bg-emerald-500 scale-110' :
                    packagesProgress > 0 ? 'bg-blue-500 animate-pulse' : 'bg-zinc-800'
                  }`}>
                    {packagesProgress === 100 ? '[OK]' : '3'}
                  </div>
                  <div className="flex-1">
                    <h3 className="text-white font-semibold">Python Libraries</h3>
                    <p className="text-zinc-500 text-sm">
                      Installing pandas, numpy, yfinance, and more...
                    </p>
                  </div>
                </div>
                <span className="text-zinc-400 text-sm font-mono">{packagesProgress}%</span>
              </div>
              <div className="w-full bg-zinc-800 rounded-full h-2 overflow-hidden">
                <div
                  className="bg-gradient-to-r from-blue-500 to-emerald-500 h-full transition-all duration-500 ease-out"
                  style={{ width: `${packagesProgress}%` }}
                />
              </div>
              {currentStep === 'packages' && currentMessage && (
                <div className="flex items-center space-x-2 text-sm">
                  <div className="w-1.5 h-1.5 bg-blue-500 rounded-full animate-pulse"></div>
                  <p className="text-blue-300 font-medium animate-fade-in">
                    {currentMessage}
                  </p>
                </div>
              )}
            </div>
          </div>
        )}

        {/* Error Display */}
        {error && (
          <div className="bg-red-900/20 border border-red-500/50 rounded-lg p-4 space-y-3">
            <div className="flex items-start space-x-3">
              <span className="text-red-500 text-xl">[WARN]</span>
              <div className="flex-1">
                <h4 className="text-red-500 font-semibold">Setup Error</h4>
                <p className="text-red-300 text-sm mt-1">{error}</p>
              </div>
            </div>
            <button
              onClick={retrySetup}
              className="w-full bg-red-600 hover:bg-red-700 text-white py-2 rounded-lg transition-colors"
            >
              Retry Setup
            </button>
          </div>
        )}

        {/* Info Box */}
        {!error && (
          <div className="bg-blue-900/20 border border-blue-500/50 rounded-lg p-4 space-y-3">
            <div className="flex items-start space-x-3">
              <svg
                className="w-5 h-5 text-blue-400 mt-0.5 flex-shrink-0"
                fill="none"
                stroke="currentColor"
                viewBox="0 0 24 24"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth={2}
                  d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"
                />
              </svg>
              <div className="flex-1 space-y-2">
                <p className="text-blue-300 text-sm">
                  <strong>Note:</strong> This is a one-time setup process. The application will download and install the necessary runtimes for optimal performance.
                </p>
                <p className="text-blue-200 text-sm font-medium">
                   This process may take a few minutes to complete. On older CPUs, it might take longer.
                </p>
                <p className="text-yellow-300 text-sm font-semibold">
                  [WARN]️ Please do not close the application during installation. Wait for the process to complete.
                </p>
              </div>
            </div>
          </div>
        )}

        {/* Installation Logs (Collapsible) */}
        {isInstalling && installationLogs.length > 0 && (
          <div className="bg-zinc-900 border border-zinc-800 rounded-lg overflow-hidden">
            <button
              onClick={() => setShowLogs(!showLogs)}
              className="w-full px-4 py-3 flex items-center justify-between text-left hover:bg-zinc-800 transition-colors"
            >
              <div className="flex items-center space-x-2">
                <svg
                  className={`w-4 h-4 text-zinc-400 transition-transform ${showLogs ? 'rotate-90' : ''}`}
                  fill="none"
                  stroke="currentColor"
                  viewBox="0 0 24 24"
                >
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 5l7 7-7 7" />
                </svg>
                <span className="text-white font-medium">Installation Logs</span>
                <span className="text-zinc-500 text-sm">({installationLogs.length} entries)</span>
              </div>
              <span className="text-zinc-500 text-xs">Click to {showLogs ? 'hide' : 'show'}</span>
            </button>
            {showLogs && (
              <div className="border-t border-zinc-800 bg-black/40 p-4 max-h-64 overflow-y-auto font-mono text-xs">
                {installationLogs.map((log, index) => (
                  <div
                    key={index}
                    className="text-zinc-400 py-0.5 hover:bg-zinc-800/50 px-2 rounded animate-fade-in"
                  >
                    <span className="text-zinc-600 mr-2">{index + 1}.</span>
                    {log}
                  </div>
                ))}
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};

export default SetupScreen;
