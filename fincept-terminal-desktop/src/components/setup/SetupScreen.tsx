// SetupScreen.tsx - Fincept-style setup screen for embedded Python
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
  needs_sync: boolean;
  sync_message: string | null;
}

interface SetupScreenProps {
  onSetupComplete: () => void;
}

// Fincept Professional Color Palette
const FINCEPT = {
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
      <div className="min-h-screen flex items-center justify-center" style={{ backgroundColor: FINCEPT.DARK_BG }}>
        <div className="text-center space-y-6">
          <div className="relative inline-block">
            <Loader2 className="w-16 h-16 animate-spin" style={{ color: FINCEPT.ORANGE }} />
          </div>
          <div>
            <h2 className="text-2xl font-bold tracking-wide" style={{ color: FINCEPT.WHITE }}>
              FINCEPT TERMINAL
            </h2>
            <p className="text-sm font-mono mt-2" style={{ color: FINCEPT.GRAY }}>
              CHECKING SYSTEM REQUIREMENTS...
            </p>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen flex items-center justify-center p-8" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      <div className="max-w-4xl w-full space-y-6">
        {/* Header */}
        <div className="text-center space-y-3 pb-6 border-b" style={{ borderColor: FINCEPT.BORDER }}>
          <div className="inline-block px-4 py-2 rounded" style={{ backgroundColor: FINCEPT.ORANGE }}>
            <h1 className="text-2xl font-bold tracking-wider" style={{ color: FINCEPT.DARK_BG }}>
              FINCEPT TERMINAL
            </h1>
          </div>
          <p className="text-sm font-mono tracking-wide" style={{ color: FINCEPT.GRAY }}>
            FIRST-TIME SETUP | INSTALLING EMBEDDED RUNTIME
          </p>
          {setupStatus && !setupStatus.needs_setup && (
            <p className="text-xs font-mono" style={{ color: FINCEPT.GREEN }}>
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
                backgroundColor: FINCEPT.ORANGE,
                color: FINCEPT.DARK_BG
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
          <div className="rounded border" style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}>
            {/* Python Installation */}
            <div className="p-6 border-b" style={{ borderColor: FINCEPT.BORDER }}>
              <div className="flex items-center justify-between mb-3">
                <div className="flex items-center space-x-4">
                  {pythonProgress === 100 ? (
                    <CheckCircle className="w-6 h-6" style={{ color: FINCEPT.GREEN }} />
                  ) : pythonProgress > 0 ? (
                    <Loader2 className="w-6 h-6 animate-spin" style={{ color: FINCEPT.ORANGE }} />
                  ) : (
                    <div className="w-6 h-6 rounded-full border-2 flex items-center justify-center" style={{ borderColor: FINCEPT.MUTED }}>
                      <span className="text-xs font-bold" style={{ color: FINCEPT.MUTED }}>1</span>
                    </div>
                  )}
                  <div>
                    <h3 className="text-sm font-bold tracking-wide" style={{ color: FINCEPT.WHITE }}>
                      PYTHON 3.12.7 EMBEDDED RUNTIME
                    </h3>
                    <p className="text-xs font-mono mt-1" style={{ color: FINCEPT.GRAY }}>
                      {setupStatus?.python_installed ? 'INSTALLED' : 'DOWNLOADING & EXTRACTING'}
                    </p>
                  </div>
                </div>
                <span className="text-xs font-mono tabular-nums" style={{ color: FINCEPT.ORANGE }}>
                  {pythonProgress}%
                </span>
              </div>
              <div className="w-full h-1 rounded-full overflow-hidden" style={{ backgroundColor: FINCEPT.BORDER }}>
                <div
                  className="h-full transition-all duration-500"
                  style={{
                    width: `${pythonProgress}%`,
                    backgroundColor: pythonProgress === 100 ? FINCEPT.GREEN : FINCEPT.ORANGE
                  }}
                />
              </div>
              {currentStep === 'python' && currentMessage && (
                <div className="flex items-center space-x-2 mt-2">
                  <div className="w-1 h-1 rounded-full animate-pulse" style={{ backgroundColor: FINCEPT.ORANGE }} />
                  <p className="text-xs font-mono" style={{ color: FINCEPT.CYAN }}>
                    {currentMessage}
                  </p>
                </div>
              )}
            </div>

            {/* Bun Installation */}
            <div className="p-6 border-b" style={{ borderColor: FINCEPT.BORDER }}>
              <div className="flex items-center justify-between mb-3">
                <div className="flex items-center space-x-4">
                  {bunProgress === 100 ? (
                    <CheckCircle className="w-6 h-6" style={{ color: FINCEPT.GREEN }} />
                  ) : bunProgress > 0 ? (
                    <Loader2 className="w-6 h-6 animate-spin" style={{ color: FINCEPT.ORANGE }} />
                  ) : (
                    <div className="w-6 h-6 rounded-full border-2 flex items-center justify-center" style={{ borderColor: FINCEPT.MUTED }}>
                      <span className="text-xs font-bold" style={{ color: FINCEPT.MUTED }}>2</span>
                    </div>
                  )}
                  <div>
                    <h3 className="text-sm font-bold tracking-wide" style={{ color: FINCEPT.WHITE }}>
                      BUN v1.1.0 RUNTIME
                    </h3>
                    <p className="text-xs font-mono mt-1" style={{ color: FINCEPT.GRAY }}>
                      {setupStatus?.bun_installed ? 'INSTALLED' : 'JAVASCRIPT RUNTIME FOR MCP SERVERS'}
                    </p>
                  </div>
                </div>
                <span className="text-xs font-mono tabular-nums" style={{ color: FINCEPT.ORANGE }}>
                  {bunProgress}%
                </span>
              </div>
              <div className="w-full h-1 rounded-full overflow-hidden" style={{ backgroundColor: FINCEPT.BORDER }}>
                <div
                  className="h-full transition-all duration-500"
                  style={{
                    width: `${bunProgress}%`,
                    backgroundColor: bunProgress === 100 ? FINCEPT.GREEN : FINCEPT.ORANGE
                  }}
                />
              </div>
              {currentStep === 'bun' && currentMessage && (
                <div className="flex items-center space-x-2 mt-2">
                  <div className="w-1 h-1 rounded-full animate-pulse" style={{ backgroundColor: FINCEPT.ORANGE }} />
                  <p className="text-xs font-mono" style={{ color: FINCEPT.CYAN }}>
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
                    <CheckCircle className="w-6 h-6" style={{ color: FINCEPT.GREEN }} />
                  ) : packagesProgress > 0 ? (
                    <Loader2 className="w-6 h-6 animate-spin" style={{ color: FINCEPT.ORANGE }} />
                  ) : (
                    <div className="w-6 h-6 rounded-full border-2 flex items-center justify-center" style={{ borderColor: FINCEPT.MUTED }}>
                      <span className="text-xs font-bold" style={{ color: FINCEPT.MUTED }}>3</span>
                    </div>
                  )}
                  <div>
                    <h3 className="text-sm font-bold tracking-wide" style={{ color: FINCEPT.WHITE }}>
                      PYTHON LIBRARIES | NUMPY 2.x
                    </h3>
                    <p className="text-xs font-mono mt-1" style={{ color: FINCEPT.GRAY }}>
                      NUMPY, PANDAS, TORCH, VNPY, LANGCHAIN, +80 PACKAGES
                    </p>
                  </div>
                </div>
                <span className="text-xs font-mono tabular-nums" style={{ color: FINCEPT.ORANGE }}>
                  {packagesProgress}%
                </span>
              </div>
              <div className="w-full h-1 rounded-full overflow-hidden" style={{ backgroundColor: FINCEPT.BORDER }}>
                <div
                  className="h-full transition-all duration-500"
                  style={{
                    width: `${packagesProgress}%`,
                    backgroundColor: packagesProgress === 100 ? FINCEPT.GREEN : FINCEPT.ORANGE
                  }}
                />
              </div>
              {currentStep === 'packages' && currentMessage && (
                <div className="flex items-center space-x-2 mt-2">
                  <div className="w-1 h-1 rounded-full animate-pulse" style={{ backgroundColor: FINCEPT.ORANGE }} />
                  <p className="text-xs font-mono" style={{ color: FINCEPT.CYAN }}>
                    {currentMessage}
                  </p>
                </div>
              )}
            </div>
          </div>
        )}

        {/* Error Display */}
        {error && (
          <div className="rounded border p-6" style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.RED }}>
            <div className="flex items-start space-x-4 mb-4">
              <AlertCircle className="w-6 h-6 flex-shrink-0" style={{ color: FINCEPT.RED }} />
              <div className="flex-1">
                <h4 className="text-sm font-bold tracking-wide mb-2" style={{ color: FINCEPT.RED }}>
                  SETUP ERROR
                </h4>
                <p className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                  {error}
                </p>
              </div>
            </div>
            <button
              onClick={retrySetup}
              className="w-full py-3 rounded font-bold text-xs tracking-wider transition-colors"
              style={{
                backgroundColor: FINCEPT.RED,
                color: FINCEPT.WHITE
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
          <div className="rounded border p-6" style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}>
            <div className="space-y-3 text-xs font-mono">
              <div className="flex items-start space-x-2">
                <span style={{ color: FINCEPT.ORANGE }}>[INFO]</span>
                <p style={{ color: FINCEPT.WHITE }}>
                  ONE-TIME SETUP: INSTALLING EMBEDDED PYTHON + NUMPY 2.x COMPATIBLE LIBRARIES
                </p>
              </div>
              <div className="flex items-start space-x-2">
                <span style={{ color: FINCEPT.CYAN }}>[TIME]</span>
                <p style={{ color: FINCEPT.GRAY }}>
                  ESTIMATED: 5-15 MINUTES DEPENDING ON NETWORK SPEED AND CPU
                </p>
              </div>
              <div className="flex items-start space-x-2">
                <span style={{ color: FINCEPT.YELLOW }}>[WARN]</span>
                <p style={{ color: FINCEPT.YELLOW }}>
                  DO NOT CLOSE APPLICATION DURING INSTALLATION
                </p>
              </div>
            </div>
          </div>
        )}

        {/* Installation Logs */}
        {isInstalling && installationLogs.length > 0 && (
          <div className="rounded border overflow-hidden" style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}>
            <button
              onClick={() => setShowLogs(!showLogs)}
              className="w-full px-6 py-4 flex items-center justify-between transition-colors"
              style={{ backgroundColor: FINCEPT.HEADER_BG }}
              onMouseEnter={(e) => e.currentTarget.style.backgroundColor = FINCEPT.HOVER}
              onMouseLeave={(e) => e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG}
            >
              <div className="flex items-center space-x-3">
                {showLogs ? (
                  <ChevronUp className="w-4 h-4" style={{ color: FINCEPT.ORANGE }} />
                ) : (
                  <ChevronDown className="w-4 h-4" style={{ color: FINCEPT.ORANGE }} />
                )}
                <span className="text-xs font-bold tracking-wide" style={{ color: FINCEPT.WHITE }}>
                  INSTALLATION LOGS
                </span>
                <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                  ({installationLogs.length})
                </span>
              </div>
              <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                {showLogs ? 'HIDE' : 'SHOW'}
              </span>
            </button>
            {showLogs && (
              <div className="border-t p-4 max-h-64 overflow-y-auto font-mono text-xs" style={{ borderColor: FINCEPT.BORDER, backgroundColor: FINCEPT.DARK_BG }}>
                {installationLogs.map((log, idx) => (
                  <div
                    key={idx}
                    className="py-1 px-2 rounded hover:bg-opacity-50 transition-colors"
                    style={{ color: FINCEPT.GRAY }}
                    onMouseEnter={(e) => e.currentTarget.style.backgroundColor = FINCEPT.HOVER}
                    onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                  >
                    <span style={{ color: FINCEPT.MUTED }}>[{idx + 1}]</span> {log}
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
