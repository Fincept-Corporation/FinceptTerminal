import { useEffect, useState, useCallback, useRef } from 'react';
import { check, type Update } from '@tauri-apps/plugin-updater';
import { relaunch } from '@tauri-apps/plugin-process';
import { getSetting, saveSetting } from '@/services/sqliteService';

const CHECK_INTERVAL_MS = 30 * 60 * 1000; // Check every 30 minutes
const LAST_CHECK_KEY = 'hook_updater_last_check';
const DISMISSED_VERSION_KEY = 'hook_updater_dismissed_version';

interface UseAutoUpdaterReturn {
  updateAvailable: boolean;
  updateInfo: Update | null;
  isChecking: boolean;
  isInstalling: boolean;
  installProgress: number;
  error: string | null;
  checkForUpdate: () => Promise<void>;
  installUpdate: () => Promise<void>;
  dismissUpdate: () => void;
}

export function useAutoUpdater(): UseAutoUpdaterReturn {
  const [updateAvailable, setUpdateAvailable] = useState(false);
  const [updateInfo, setUpdateInfo] = useState<Update | null>(null);
  const [isChecking, setIsChecking] = useState(false);
  const [isInstalling, setIsInstalling] = useState(false);
  const [installProgress, setInstallProgress] = useState(0);
  const [error, setError] = useState<string | null>(null);

  const checkingRef = useRef<boolean>(false);
  const relaunchTimerRef = useRef<NodeJS.Timeout | undefined>(undefined);

  const checkForUpdate = useCallback(async (silent = false) => {
    // Prevent duplicate checks
    if (checkingRef.current) {
      console.log('[AutoUpdater] Check already in progress, skipping');
      return;
    }

    try {
      checkingRef.current = true;
      setIsChecking(true);
      setError(null);

      const update = await check();

      if (update?.available) {
        console.log('[AutoUpdater] Update available:', update.version);

        // PURE SQLite - Check if this version was previously dismissed
        const dismissedVersion = await getSetting(DISMISSED_VERSION_KEY);
        if (dismissedVersion === update.version) {
          console.log('[AutoUpdater] Update was dismissed by user');
          if (!silent) {
            // Don't show as error, just log
            console.log('[AutoUpdater] This update was previously dismissed');
          }
          return;
        }

        setUpdateInfo(update);
        setUpdateAvailable(true);

        // PURE SQLite - Update last check timestamp
        await saveSetting(LAST_CHECK_KEY, Date.now().toString(), 'auto_updater');
      } else {
        console.log('[AutoUpdater] No updates available');
        if (!silent) {
          setError('You are running the latest version!');
          setTimeout(() => setError(null), 3000);
        }
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Unknown error occurred';
      console.error('[AutoUpdater] Error checking for updates:', errorMessage, err);

      // Silently fail for development/network errors
      if (!silent) {
        // Only show user-facing errors for non-network issues
        if (!errorMessage.toLowerCase().includes('network') &&
            !errorMessage.toLowerCase().includes('fetch') &&
            !errorMessage.toLowerCase().includes('failed to fetch')) {
          setError(`Failed to check for updates: ${errorMessage}`);
        } else {
          console.log('[AutoUpdater] Network error during update check (silent)');
        }
      }
    } finally {
      setIsChecking(false);
      checkingRef.current = false;
    }
  }, []);

  const installUpdate = useCallback(async () => {
    if (!updateInfo) {
      console.error('[AutoUpdater] No update info available');
      setError('No update information available');
      return;
    }

    try {
      setIsInstalling(true);
      setInstallProgress(0);
      setError(null);

      // Download and install with progress tracking
      let totalBytes = 0;
      let downloadedBytes = 0;

      await updateInfo.downloadAndInstall((event) => {
        switch (event.event) {
          case 'Started':
            totalBytes = (event.data as any).contentLength || 0;
            downloadedBytes = 0;
            setInstallProgress(0);
            console.log('[AutoUpdater] Download started, size:', totalBytes);
            break;
          case 'Progress':
            downloadedBytes += event.data.chunkLength;
            const progress = totalBytes > 0 ? (downloadedBytes / totalBytes) * 100 : 0;
            setInstallProgress(Math.min(progress, 100));
            break;
          case 'Finished':
            setInstallProgress(100);
            console.log('[AutoUpdater] Download finished');
            break;
        }
      });

      console.log('[AutoUpdater] Update installed, relaunching in 2s...');

      // Clear any existing relaunch timer
      if (relaunchTimerRef.current) {
        clearTimeout(relaunchTimerRef.current);
      }

      // Give user a moment to see the success message
      relaunchTimerRef.current = setTimeout(async () => {
        try {
          await relaunch();
        } catch (relaunchError) {
          console.error('[AutoUpdater] Failed to relaunch:', relaunchError);
          setError('Update installed but failed to relaunch. Please restart manually.');
          setIsInstalling(false);
        }
      }, 2000);

    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Unknown error occurred';
      console.error('[AutoUpdater] Error installing update:', errorMessage);
      setError(`Failed to install update: ${errorMessage}`);
      setIsInstalling(false);
      setInstallProgress(0);
    }
  }, [updateInfo]);

  const dismissUpdate = useCallback(async () => {
    if (updateInfo?.version) {
      // PURE SQLite - Store the dismissed version so we don't show it again
      await saveSetting(DISMISSED_VERSION_KEY, updateInfo.version, 'auto_updater');
      console.log('[AutoUpdater] Update dismissed:', updateInfo.version);
    }
    setUpdateAvailable(false);
    setUpdateInfo(null);
    setError(null);
  }, [updateInfo]);

  useEffect(() => {
    console.log('[AutoUpdater] Hook initialized');

    // Perform initial check on mount (delayed by 10 seconds to not interfere with app startup)
    const initialCheckTimer = setTimeout(() => {
      console.log('[AutoUpdater] Running initial update check...');
      checkForUpdate(true);
    }, 10000); // Increased delay to 10s

    // Set up periodic checks every 30 minutes
    const interval = setInterval(() => {
      console.log('[AutoUpdater] Running periodic update check...');
      checkForUpdate(true);
    }, CHECK_INTERVAL_MS);

    return () => {
      console.log('[AutoUpdater] Hook cleanup');
      clearTimeout(initialCheckTimer);
      clearInterval(interval);
      if (relaunchTimerRef.current) {
        clearTimeout(relaunchTimerRef.current);
      }
    };
  }, [checkForUpdate]);

  return {
    updateAvailable,
    updateInfo,
    isChecking,
    isInstalling,
    installProgress,
    error,
    checkForUpdate: () => checkForUpdate(false),
    installUpdate,
    dismissUpdate,
  };
}
