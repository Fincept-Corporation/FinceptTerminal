import { useEffect, useState } from 'react';
import { check, type Update } from '@tauri-apps/plugin-updater';
import { relaunch } from '@tauri-apps/plugin-process';

const CHECK_INTERVAL_MS = 30 * 60 * 1000; // Check every 30 minutes
const LAST_CHECK_KEY = 'updater_last_check';
const DISMISSED_VERSION_KEY = 'updater_dismissed_version';

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

  const checkForUpdate = async (silent = false) => {
    try {
      setIsChecking(true);
      setError(null);

      const update = await check();

      if (update?.available) {

        // Check if this version was previously dismissed
        const dismissedVersion = localStorage.getItem(DISMISSED_VERSION_KEY);
        if (dismissedVersion === update.version) {
          if (!silent) {
            setError('This update was previously dismissed. A new update will be shown when available.');
          }
          return;
        }

        setUpdateInfo(update);
        setUpdateAvailable(true);

        // Update last check timestamp
        localStorage.setItem(LAST_CHECK_KEY, Date.now().toString());
      } else {
        if (!silent) {
          setError('You are running the latest version!');
          setTimeout(() => setError(null), 3000);
        }
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Unknown error occurred';
      console.error('[AutoUpdater] Error checking for updates:', errorMessage);

      // Only show error if it's not a network-related error or if not silent
      if (!silent && !errorMessage.toLowerCase().includes('network') && !errorMessage.toLowerCase().includes('fetch')) {
        setError(`Failed to check for updates: ${errorMessage}`);
      }
    } finally {
      setIsChecking(false);
    }
  };

  const installUpdate = async () => {
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
            break;
          case 'Progress':
            downloadedBytes += event.data.chunkLength;
            const progress = totalBytes > 0 ? (downloadedBytes / totalBytes) * 100 : 0;
            setInstallProgress(Math.min(progress, 100));
            break;
          case 'Finished':
            setInstallProgress(100);
            break;
        }
      });


      // Give user a moment to see the success message
      setTimeout(async () => {
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
  };

  const dismissUpdate = () => {
    if (updateInfo?.version) {
      // Store the dismissed version so we don't show it again
      localStorage.setItem(DISMISSED_VERSION_KEY, updateInfo.version);
    }
    setUpdateAvailable(false);
    setUpdateInfo(null);
    setError(null);
  };

  useEffect(() => {
    // Perform initial check on mount (delayed by 5 seconds to not interfere with app startup)
    const initialCheckTimer = setTimeout(() => {
      checkForUpdate(true);
    }, 5000);

    // Set up periodic checks every 30 minutes
    const interval = setInterval(() => {
      checkForUpdate(true);
    }, CHECK_INTERVAL_MS);

    return () => {
      clearTimeout(initialCheckTimer);
      clearInterval(interval);
    };
  }, []);

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
