import { useEffect, useState, useRef, useCallback } from 'react';
import { check, Update } from '@tauri-apps/plugin-updater';
import { relaunch } from '@tauri-apps/plugin-process';
import { Download, RefreshCw, CheckCircle2, XCircle, Info, ArrowUpCircle, X } from 'lucide-react';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
};

interface AutoUpdaterProps {
  checkOnMount?: boolean;
  checkIntervalMinutes?: number;
}

export function AutoUpdater({ checkOnMount = true, checkIntervalMinutes = 30 }: AutoUpdaterProps) {
  const [updateAvailable, setUpdateAvailable] = useState(false);
  const [updateInfo, setUpdateInfo] = useState<Update | null>(null);
  const [downloading, setDownloading] = useState(false);
  const [downloadProgress, setDownloadProgress] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [checking, setChecking] = useState(false);
  const [showDialog, setShowDialog] = useState(false);
  const [updateReady, setUpdateReady] = useState(false);

  const checkingRef = useRef<boolean>(false);
  const relaunchTimerRef = useRef<NodeJS.Timeout | undefined>(undefined);

  const checkForUpdates = useCallback(async (silent = false) => {
    if (checkingRef.current) {
      console.log('[AutoUpdater Component] Check already in progress, skipping');
      return;
    }

    try {
      checkingRef.current = true;
      setChecking(true);
      setError(null);

      console.log('[AutoUpdater Component] Checking for updates...');
      const update = await check();

      if (update?.available) {
        console.log(`[AutoUpdater Component] Update available: v${update.version}`);
        console.log(`[AutoUpdater Component] Current version: v${update.currentVersion}`);
        console.log(`[AutoUpdater Component] Release date: ${update.date}`);

        setUpdateInfo(update);
        setUpdateAvailable(true);

        if (!silent) {
          setShowDialog(true);
        }
      } else {
        console.log('[AutoUpdater Component] No updates available');

        if (!silent) {
          setError('You are running the latest version!');
          setTimeout(() => setError(null), 3000);
        }
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Unknown error occurred';
      console.error('[AutoUpdater Component] Error checking for updates:', errorMessage, err);

      if (!silent) {
        if (!errorMessage.toLowerCase().includes('network') &&
            !errorMessage.toLowerCase().includes('fetch') &&
            !errorMessage.toLowerCase().includes('failed to fetch')) {
          setError(`Failed to check for updates: ${errorMessage}`);
        } else {
          console.log('[AutoUpdater Component] Network error during update check (silent)');
        }
      }
    } finally {
      setChecking(false);
      checkingRef.current = false;
    }
  }, []);

  const downloadAndInstall = useCallback(async () => {
    if (!updateInfo) return;

    try {
      setDownloading(true);
      setError(null);
      setDownloadProgress(0);

      console.log('[AutoUpdater Component] Starting download and installation...');

      let totalBytes = 0;
      let downloadedBytes = 0;

      await updateInfo.downloadAndInstall((event) => {
        switch (event.event) {
          case 'Started':
            totalBytes = (event.data as any).contentLength || 0;
            downloadedBytes = 0;
            console.log(`[AutoUpdater Component] Download started - Total size: ${totalBytes} bytes`);
            setDownloadProgress(0);
            break;
          case 'Progress':
            downloadedBytes += event.data.chunkLength;
            const progress = totalBytes > 0 ? (downloadedBytes / totalBytes) * 100 : 0;
            setDownloadProgress(progress);
            break;
          case 'Finished':
            console.log('[AutoUpdater Component] Download finished, installing...');
            setDownloadProgress(100);
            break;
        }
      });

      console.log('[AutoUpdater Component] Update installed successfully! Preparing to relaunch...');
      setUpdateReady(true);

      if (relaunchTimerRef.current) {
        clearTimeout(relaunchTimerRef.current);
      }

      relaunchTimerRef.current = setTimeout(async () => {
        console.log('[AutoUpdater Component] Relaunching application...');
        await relaunch();
      }, 2000);

    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Unknown error occurred';
      console.error('[AutoUpdater Component] Error during download/install:', errorMessage);
      setError(`Failed to install update: ${errorMessage}`);
      setDownloading(false);
    }
  }, [updateInfo]);

  const handleLater = useCallback(() => {
    setShowDialog(false);
    console.log('[AutoUpdater Component] Update postponed by user');
  }, []);

  useEffect(() => {
    if (checkOnMount) {
      console.log('[AutoUpdater Component] Initialized');
      const timer = setTimeout(() => {
        console.log('[AutoUpdater Component] Running initial update check...');
        checkForUpdates(true);
      }, 15000);

      return () => {
        clearTimeout(timer);
        if (relaunchTimerRef.current) {
          clearTimeout(relaunchTimerRef.current);
        }
      };
    }
  }, [checkOnMount, checkForUpdates]);

  useEffect(() => {
    if (checkIntervalMinutes > 0) {
      const interval = setInterval(() => {
        console.log('[AutoUpdater Component] Running periodic update check...');
        checkForUpdates(true);
      }, checkIntervalMinutes * 60 * 1000);

      return () => clearInterval(interval);
    }
  }, [checkIntervalMinutes, checkForUpdates]);

  const formatBytes = (bytes: number) => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
  };

  return (
    <>
      {/* Update Dialog - Overlay */}
      {showDialog && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999,
        }}>
          <div style={{
            width: '460px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            overflow: 'hidden',
            boxShadow: `0 4px 24px rgba(0, 0, 0, 0.6), 0 0 1px ${FINCEPT.ORANGE}40`,
          }}>
            {/* Header */}
            <div style={{
              padding: '12px 16px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `2px solid ${FINCEPT.ORANGE}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <ArrowUpCircle size={14} color={FINCEPT.ORANGE} />
                <span style={{
                  fontSize: '11px',
                  fontWeight: 700,
                  color: FINCEPT.WHITE,
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                  letterSpacing: '0.5px',
                }}>
                  UPDATE AVAILABLE
                </span>
              </div>
              {!downloading && !updateReady && (
                <button
                  onClick={handleLater}
                  style={{
                    background: 'none',
                    border: 'none',
                    cursor: 'pointer',
                    padding: '2px',
                    display: 'flex',
                    alignItems: 'center',
                  }}
                >
                  <X size={14} color={FINCEPT.GRAY} />
                </button>
              )}
            </div>

            {/* Body */}
            <div style={{ padding: '16px' }}>
              {/* Version Info */}
              {updateInfo && (
                <div style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  padding: '12px',
                  marginBottom: '12px',
                }}>
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    marginBottom: '8px',
                  }}>
                    <span style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: FINCEPT.GRAY,
                      letterSpacing: '0.5px',
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                    }}>
                      CURRENT VERSION
                    </span>
                    <span style={{
                      fontSize: '10px',
                      color: FINCEPT.MUTED,
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                    }}>
                      v{updateInfo.currentVersion}
                    </span>
                  </div>
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    marginBottom: '8px',
                  }}>
                    <span style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: FINCEPT.GRAY,
                      letterSpacing: '0.5px',
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                    }}>
                      NEW VERSION
                    </span>
                    <span style={{
                      fontSize: '10px',
                      color: FINCEPT.GREEN,
                      fontWeight: 700,
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                    }}>
                      v{updateInfo.version}
                    </span>
                  </div>
                  {updateInfo.date && (
                    <div style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                    }}>
                      <span style={{
                        fontSize: '9px',
                        fontWeight: 700,
                        color: FINCEPT.GRAY,
                        letterSpacing: '0.5px',
                        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                      }}>
                        RELEASE DATE
                      </span>
                      <span style={{
                        fontSize: '10px',
                        color: FINCEPT.CYAN,
                        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                      }}>
                        {new Date(updateInfo.date).toLocaleDateString()}
                      </span>
                    </div>
                  )}
                </div>
              )}

              {/* Release Notes */}
              {updateInfo?.body && (
                <div style={{ marginBottom: '12px' }}>
                  <span style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                    display: 'block',
                    marginBottom: '6px',
                  }}>
                    RELEASE NOTES
                  </span>
                  <div style={{
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    padding: '10px',
                    maxHeight: '120px',
                    overflowY: 'auto',
                  }}>
                    <pre style={{
                      fontSize: '10px',
                      color: FINCEPT.WHITE,
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                      whiteSpace: 'pre-wrap',
                      wordBreak: 'break-word',
                      margin: 0,
                      lineHeight: '1.5',
                    }}>
                      {updateInfo.body}
                    </pre>
                  </div>
                </div>
              )}

              {/* Download Progress */}
              {downloading && !updateReady && (
                <div style={{ marginBottom: '12px' }}>
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    marginBottom: '6px',
                  }}>
                    <span style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: FINCEPT.GRAY,
                      letterSpacing: '0.5px',
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                    }}>
                      {downloadProgress === 100 ? 'INSTALLING' : 'DOWNLOADING'}
                    </span>
                    <span style={{
                      fontSize: '10px',
                      color: FINCEPT.ORANGE,
                      fontWeight: 700,
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                    }}>
                      {downloadProgress.toFixed(0)}%
                    </span>
                  </div>
                  {/* Custom Progress Bar */}
                  <div style={{
                    width: '100%',
                    height: '4px',
                    backgroundColor: FINCEPT.DARK_BG,
                    borderRadius: '2px',
                    overflow: 'hidden',
                    border: `1px solid ${FINCEPT.BORDER}`,
                  }}>
                    <div style={{
                      width: `${downloadProgress}%`,
                      height: '100%',
                      backgroundColor: FINCEPT.ORANGE,
                      transition: 'width 0.3s ease',
                      borderRadius: '2px',
                    }} />
                  </div>
                </div>
              )}

              {/* Update Ready */}
              {updateReady && (
                <div style={{
                  padding: '10px 12px',
                  backgroundColor: `${FINCEPT.GREEN}15`,
                  border: `1px solid ${FINCEPT.GREEN}40`,
                  borderRadius: '2px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                  marginBottom: '12px',
                }}>
                  <CheckCircle2 size={14} color={FINCEPT.GREEN} />
                  <div>
                    <span style={{
                      fontSize: '10px',
                      fontWeight: 700,
                      color: FINCEPT.GREEN,
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                      display: 'block',
                    }}>
                      UPDATE INSTALLED
                    </span>
                    <span style={{
                      fontSize: '9px',
                      color: FINCEPT.GRAY,
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                    }}>
                      Restarting application...
                    </span>
                  </div>
                </div>
              )}

              {/* Error */}
              {error && (
                <div style={{
                  padding: '10px 12px',
                  backgroundColor: `${FINCEPT.RED}15`,
                  border: `1px solid ${FINCEPT.RED}40`,
                  borderRadius: '2px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                  marginBottom: '12px',
                }}>
                  <XCircle size={14} color={FINCEPT.RED} />
                  <span style={{
                    fontSize: '10px',
                    color: FINCEPT.RED,
                    fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                  }}>
                    {error}
                  </span>
                </div>
              )}

              {/* Info text */}
              {!downloading && !updateReady && !error && (
                <span style={{
                  fontSize: '9px',
                  color: FINCEPT.MUTED,
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                }}>
                  The update will download and install automatically. Your app will restart after installation.
                </span>
              )}
            </div>

            {/* Footer */}
            <div style={{
              padding: '12px 16px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              justifyContent: 'flex-end',
              gap: '8px',
            }}>
              {!downloading && !updateReady && (
                <>
                  <button
                    onClick={handleLater}
                    style={{
                      padding: '6px 10px',
                      backgroundColor: 'transparent',
                      border: `1px solid ${FINCEPT.BORDER}`,
                      color: FINCEPT.GRAY,
                      fontSize: '9px',
                      fontWeight: 700,
                      borderRadius: '2px',
                      cursor: 'pointer',
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                      letterSpacing: '0.5px',
                      transition: 'all 0.2s',
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                      e.currentTarget.style.color = FINCEPT.WHITE;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                      e.currentTarget.style.color = FINCEPT.GRAY;
                    }}
                  >
                    LATER
                  </button>
                  <button
                    onClick={downloadAndInstall}
                    disabled={downloading}
                    style={{
                      padding: '6px 14px',
                      backgroundColor: FINCEPT.ORANGE,
                      color: FINCEPT.DARK_BG,
                      border: 'none',
                      borderRadius: '2px',
                      fontSize: '9px',
                      fontWeight: 700,
                      cursor: 'pointer',
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                      letterSpacing: '0.5px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      transition: 'all 0.2s',
                    }}
                  >
                    <Download size={10} />
                    UPDATE NOW
                  </button>
                </>
              )}
              {downloading && !updateReady && (
                <button
                  disabled
                  style={{
                    padding: '6px 14px',
                    backgroundColor: FINCEPT.MUTED,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                    fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                    letterSpacing: '0.5px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    cursor: 'not-allowed',
                    opacity: 0.7,
                  }}
                >
                  <RefreshCw size={10} className="animate-spin" />
                  {downloadProgress === 100 ? 'INSTALLING...' : 'DOWNLOADING...'}
                </button>
              )}
            </div>
          </div>
        </div>
      )}

      {/* Manual Check Button */}
      <button
        onClick={() => checkForUpdates(false)}
        disabled={checking || downloading}
        style={{
          padding: '6px 10px',
          backgroundColor: 'transparent',
          border: `1px solid ${FINCEPT.BORDER}`,
          color: FINCEPT.GRAY,
          fontSize: '9px',
          fontWeight: 700,
          borderRadius: '2px',
          cursor: checking || downloading ? 'not-allowed' : 'pointer',
          fontFamily: '"IBM Plex Mono", "Consolas", monospace',
          letterSpacing: '0.5px',
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          transition: 'all 0.2s',
          opacity: checking || downloading ? 0.5 : 1,
        }}
      >
        {checking ? (
          <>
            <RefreshCw size={10} className="animate-spin" />
            CHECKING...
          </>
        ) : (
          <>
            <Info size={10} />
            CHECK FOR UPDATES
          </>
        )}
      </button>

      {/* Toast notification */}
      {error && !showDialog && (
        <div style={{
          position: 'fixed',
          bottom: '16px',
          right: '16px',
          zIndex: 9999,
          maxWidth: '320px',
          padding: '10px 12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${error.includes('latest version') ? FINCEPT.GREEN + '40' : FINCEPT.RED + '40'}`,
          borderRadius: '2px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          boxShadow: '0 4px 12px rgba(0, 0, 0, 0.4)',
        }}>
          {error.includes('latest version') ? (
            <CheckCircle2 size={12} color={FINCEPT.GREEN} />
          ) : (
            <XCircle size={12} color={FINCEPT.RED} />
          )}
          <span style={{
            fontSize: '10px',
            color: error.includes('latest version') ? FINCEPT.GREEN : FINCEPT.RED,
            fontFamily: '"IBM Plex Mono", "Consolas", monospace',
          }}>
            {error}
          </span>
        </div>
      )}
    </>
  );
}
