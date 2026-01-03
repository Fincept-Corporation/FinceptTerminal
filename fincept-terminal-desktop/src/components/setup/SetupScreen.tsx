// SetupScreen.tsx - Bloomberg-style setup screen for embedded Python
import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';
import { Loader2, CheckCircle, AlertCircle, ChevronDown, ChevronUp } from 'lucide-react';

interface SetupProgress {
  step: string;
  progress: number;
  message: string;
  is_error: boolean;
}

interface SetupStatus {
  python_installed: boolean;
  bun_installed: boolean;
  packages_installed: boolean;
  needs_setup: boolean;
}

interface SetupScreenProps {
  onSetupComplete: () => void;
}

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

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
        setTimeout(() => onSetupComplete(), 1000);
      } else {
        setIsChecking(false);
        // Don't auto-run setup, let user trigger it
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

      if (progress.message) {
        setInstallationLogs(prev => [...prev, `[${progress.step}] ${progress.message}`]);
      }

      if (progress.is_error) {
        setError(progress.message);
        return;
      }

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
      <div className="min-h-screen flex items-center justify-center" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
        <div className="text-center space-y-6">
          <div className="relative inline-block">
            <Loader2 className="w-16 h-16 animate-spin" style={{ color: BLOOMBERG.ORANGE }} />
          </div>
          <div>
            <h2 className="text-2xl font-bold tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
              FINCEPT TERMINAL
            </h2>
            <p className="text-sm font-mono mt-2" style={{ color: BLOOMBERG.GRAY }}>
              CHECKING SYSTEM REQUIREMENTS...
            </p>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen flex items-center justify-center p-8" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
      <div className="max-w-4xl w-full space-y-6">
        {/* Header */}
        <div className="text-center space-y-3 pb-6 border-b" style={{ borderColor: BLOOMBERG.BORDER }}>
          <div className="inline-block px-4 py-2 rounded" style={{ backgroundColor: BLOOMBERG.ORANGE }}>
            <h1 className="text-2xl font-bold tracking-wider" style={{ color: BLOOMBERG.DARK_BG }}>
              FINCEPT TERMINAL
            </h1>
          </div>
          <p className="text-sm font-mono tracking-wide" style={{ color: BLOOMBERG.GRAY }}>
            FIRST-TIME SETUP | INSTALLING EMBEDDED RUNTIME
          </p>
          {setupStatus && !setupStatus.needs_setup && (
            <p className="text-xs font-mono" style={{ color: BLOOMBERG.GREEN }}>
              [OK] SYSTEM READY
            </p>
          )}
        </div>

        {/* Begin Setup Button */}
        {!isInstalling && setupStatus?.needs_setup && (
          <div className="text-center">
            <button
              onClick={runSetup}
              className="px-8 py-4 rounded font-bold text-sm tracking-wider transition-all"
              style={{
                backgroundColor: BLOOMBERG.ORANGE,
                color: BLOOMBERG.DARK_BG
              }}
              onMouseEnter={(e) => e.currentTarget.style.transform = 'scale(1.05)'}
              onMouseLeave={(e) => e.currentTarget.style.transform = 'scale(1)'}
            >
              BEGIN SETUP
            </button>
          </div>
        )}

        {/* Setup Progress */}
        {isInstalling && (
          <div className="rounded border" style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}>
            {/* Python Installation */}
            <div className="p-6 border-b" style={{ borderColor: BLOOMBERG.BORDER }}>
              <div className="flex items-center justify-between mb-3">
                <div className="flex items-center space-x-4">
                  {pythonProgress === 100 ? (
                    <CheckCircle className="w-6 h-6" style={{ color: BLOOMBERG.GREEN }} />
                  ) : pythonProgress > 0 ? (
                    <Loader2 className="w-6 h-6 animate-spin" style={{ color: BLOOMBERG.ORANGE }} />
                  ) : (
                    <div className="w-6 h-6 rounded-full border-2 flex items-center justify-center" style={{ borderColor: BLOOMBERG.MUTED }}>
                      <span className="text-xs font-bold" style={{ color: BLOOMBERG.MUTED }}>1</span>
                    </div>
                  )}
                  <div>
                    <h3 className="text-sm font-bold tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      PYTHON 3.12.7 EMBEDDED RUNTIME
                    </h3>
                    <p className="text-xs font-mono mt-1" style={{ color: BLOOMBERG.GRAY }}>
                      {setupStatus?.python_installed ? 'INSTALLED' : 'DOWNLOADING & EXTRACTING'}
                    </p>
                  </div>
                </div>
                <span className="text-xs font-mono tabular-nums" style={{ color: BLOOMBERG.ORANGE }}>
                  {pythonProgress}%
                </span>
              </div>
              <div className="w-full h-1 rounded-full overflow-hidden" style={{ backgroundColor: BLOOMBERG.BORDER }}>
                <div
                  className="h-full transition-all duration-500"
                  style={{
                    width: `${pythonProgress}%`,
                    backgroundColor: pythonProgress === 100 ? BLOOMBERG.GREEN : BLOOMBERG.ORANGE
                  }}
                />
              </div>
              {currentStep === 'python' && currentMessage && (
                <div className="flex items-center space-x-2 mt-2">
                  <div className="w-1 h-1 rounded-full animate-pulse" style={{ backgroundColor: BLOOMBERG.ORANGE }} />
                  <p className="text-xs font-mono" style={{ color: BLOOMBERG.CYAN }}>
                    {currentMessage}
                  </p>
                </div>
              )}
            </div>

            {/* Bun Installation */}
            <div className="p-6 border-b" style={{ borderColor: BLOOMBERG.BORDER }}>
              <div className="flex items-center justify-between mb-3">
                <div className="flex items-center space-x-4">
                  {bunProgress === 100 ? (
                    <CheckCircle className="w-6 h-6" style={{ color: BLOOMBERG.GREEN }} />
                  ) : bunProgress > 0 ? (
                    <Loader2 className="w-6 h-6 animate-spin" style={{ color: BLOOMBERG.ORANGE }} />
                  ) : (
                    <div className="w-6 h-6 rounded-full border-2 flex items-center justify-center" style={{ borderColor: BLOOMBERG.MUTED }}>
                      <span className="text-xs font-bold" style={{ color: BLOOMBERG.MUTED }}>2</span>
                    </div>
                  )}
                  <div>
                    <h3 className="text-sm font-bold tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      BUN v1.1.0 RUNTIME
                    </h3>
                    <p className="text-xs font-mono mt-1" style={{ color: BLOOMBERG.GRAY }}>
                      {setupStatus?.bun_installed ? 'INSTALLED' : 'JAVASCRIPT RUNTIME FOR MCP SERVERS'}
                    </p>
                  </div>
                </div>
                <span className="text-xs font-mono tabular-nums" style={{ color: BLOOMBERG.ORANGE }}>
                  {bunProgress}%
                </span>
              </div>
              <div className="w-full h-1 rounded-full overflow-hidden" style={{ backgroundColor: BLOOMBERG.BORDER }}>
                <div
                  className="h-full transition-all duration-500"
                  style={{
                    width: `${bunProgress}%`,
                    backgroundColor: bunProgress === 100 ? BLOOMBERG.GREEN : BLOOMBERG.ORANGE
                  }}
                />
              </div>
              {currentStep === 'bun' && currentMessage && (
                <div className="flex items-center space-x-2 mt-2">
                  <div className="w-1 h-1 rounded-full animate-pulse" style={{ backgroundColor: BLOOMBERG.ORANGE }} />
                  <p className="text-xs font-mono" style={{ color: BLOOMBERG.CYAN }}>
                    {currentMessage}
                  </p>
                </div>
              )}
            </div>

            {/* Python Packages */}
            <div className="p-6">
              <div className="flex items-center justify-between mb-3">
                <div className="flex items-center space-x-4">
                  {packagesProgress === 100 ? (
                    <CheckCircle className="w-6 h-6" style={{ color: BLOOMBERG.GREEN }} />
                  ) : packagesProgress > 0 ? (
                    <Loader2 className="w-6 h-6 animate-spin" style={{ color: BLOOMBERG.ORANGE }} />
                  ) : (
                    <div className="w-6 h-6 rounded-full border-2 flex items-center justify-center" style={{ borderColor: BLOOMBERG.MUTED }}>
                      <span className="text-xs font-bold" style={{ color: BLOOMBERG.MUTED }}>3</span>
                    </div>
                  )}
                  <div>
                    <h3 className="text-sm font-bold tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      PYTHON LIBRARIES | NUMPY 2.x
                    </h3>
                    <p className="text-xs font-mono mt-1" style={{ color: BLOOMBERG.GRAY }}>
                      NUMPY, PANDAS, TORCH, VNPY, LANGCHAIN, +80 PACKAGES
                    </p>
                  </div>
                </div>
                <span className="text-xs font-mono tabular-nums" style={{ color: BLOOMBERG.ORANGE }}>
                  {packagesProgress}%
                </span>
              </div>
              <div className="w-full h-1 rounded-full overflow-hidden" style={{ backgroundColor: BLOOMBERG.BORDER }}>
                <div
                  className="h-full transition-all duration-500"
                  style={{
                    width: `${packagesProgress}%`,
                    backgroundColor: packagesProgress === 100 ? BLOOMBERG.GREEN : BLOOMBERG.ORANGE
                  }}
                />
              </div>
              {currentStep === 'packages' && currentMessage && (
                <div className="flex items-center space-x-2 mt-2">
                  <div className="w-1 h-1 rounded-full animate-pulse" style={{ backgroundColor: BLOOMBERG.ORANGE }} />
                  <p className="text-xs font-mono" style={{ color: BLOOMBERG.CYAN }}>
                    {currentMessage}
                  </p>
                </div>
              )}
            </div>
          </div>
        )}

        {/* Error Display */}
        {error && (
          <div className="rounded border p-6" style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.RED }}>
            <div className="flex items-start space-x-4 mb-4">
              <AlertCircle className="w-6 h-6 flex-shrink-0" style={{ color: BLOOMBERG.RED }} />
              <div className="flex-1">
                <h4 className="text-sm font-bold tracking-wide mb-2" style={{ color: BLOOMBERG.RED }}>
                  SETUP ERROR
                </h4>
                <p className="text-xs font-mono" style={{ color: BLOOMBERG.WHITE }}>
                  {error}
                </p>
              </div>
            </div>
            <button
              onClick={retrySetup}
              className="w-full py-3 rounded font-bold text-xs tracking-wider transition-colors"
              style={{
                backgroundColor: BLOOMBERG.RED,
                color: BLOOMBERG.WHITE
              }}
              onMouseEnter={(e) => e.currentTarget.style.opacity = '0.9'}
              onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
            >
              RETRY SETUP
            </button>
          </div>
        )}

        {/* Info Box */}
        {!error && (
          <div className="rounded border p-6" style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}>
            <div className="space-y-3 text-xs font-mono">
              <div className="flex items-start space-x-2">
                <span style={{ color: BLOOMBERG.ORANGE }}>[INFO]</span>
                <p style={{ color: BLOOMBERG.WHITE }}>
                  ONE-TIME SETUP: INSTALLING EMBEDDED PYTHON + NUMPY 2.x COMPATIBLE LIBRARIES
                </p>
              </div>
              <div className="flex items-start space-x-2">
                <span style={{ color: BLOOMBERG.CYAN }}>[TIME]</span>
                <p style={{ color: BLOOMBERG.GRAY }}>
                  ESTIMATED: 5-15 MINUTES DEPENDING ON NETWORK SPEED AND CPU
                </p>
              </div>
              <div className="flex items-start space-x-2">
                <span style={{ color: BLOOMBERG.YELLOW }}>[WARN]</span>
                <p style={{ color: BLOOMBERG.YELLOW }}>
                  DO NOT CLOSE APPLICATION DURING INSTALLATION
                </p>
              </div>
            </div>
          </div>
        )}

        {/* Installation Logs */}
        {isInstalling && installationLogs.length > 0 && (
          <div className="rounded border overflow-hidden" style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}>
            <button
              onClick={() => setShowLogs(!showLogs)}
              className="w-full px-6 py-4 flex items-center justify-between transition-colors"
              style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
              onMouseEnter={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER}
              onMouseLeave={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG.HEADER_BG}
            >
              <div className="flex items-center space-x-3">
                {showLogs ? (
                  <ChevronUp className="w-4 h-4" style={{ color: BLOOMBERG.ORANGE }} />
                ) : (
                  <ChevronDown className="w-4 h-4" style={{ color: BLOOMBERG.ORANGE }} />
                )}
                <span className="text-xs font-bold tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                  INSTALLATION LOGS
                </span>
                <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>
                  ({installationLogs.length})
                </span>
              </div>
              <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>
                {showLogs ? 'HIDE' : 'SHOW'}
              </span>
            </button>
            {showLogs && (
              <div className="border-t p-4 max-h-64 overflow-y-auto font-mono text-xs" style={{ borderColor: BLOOMBERG.BORDER, backgroundColor: BLOOMBERG.DARK_BG }}>
                {installationLogs.map((log, idx) => (
                  <div
                    key={idx}
                    className="py-1 px-2 rounded hover:bg-opacity-50 transition-colors"
                    style={{ color: BLOOMBERG.GRAY }}
                    onMouseEnter={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER}
                    onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                  >
                    <span style={{ color: BLOOMBERG.MUTED }}>[{idx + 1}]</span> {log}
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
