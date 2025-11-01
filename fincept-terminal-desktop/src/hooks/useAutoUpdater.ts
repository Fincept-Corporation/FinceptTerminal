import { useEffect, useState } from 'react';
import { check, type Update } from '@tauri-apps/plugin-updater';
import { relaunch } from '@tauri-apps/plugin-process';

const SEVEN_DAYS_MS = 7 * 24 * 60 * 60 * 1000; // 7 days in milliseconds
const LAST_CHECK_KEY = 'updater_last_check';

interface UseAutoUpdaterReturn {
  updateAvailable: boolean;
  updateInfo: Update | null;
  isChecking: boolean;
  checkForUpdate: () => Promise<void>;
  installUpdate: () => Promise<void>;
  dismissUpdate: () => void;
}

export function useAutoUpdater(): UseAutoUpdaterReturn {
  const [updateAvailable, setUpdateAvailable] = useState(false);
  const [updateInfo, setUpdateInfo] = useState<Update | null>(null);
  const [isChecking, setIsChecking] = useState(false);

  const checkForUpdate = async (silent = false) => {
    try {
      setIsChecking(true);

      const update = await check();

      if (update?.available) {
        console.log(`[Updater] Update available: v${update.version}`);
        console.log(`[Updater] Release notes: ${update.body}`);

        setUpdateInfo(update);
        setUpdateAvailable(true);

        // Update last check timestamp
        localStorage.setItem(LAST_CHECK_KEY, Date.now().toString());
      } else {
        if (!silent) {
          console.log('[Updater] No updates available');
        }
      }
    } catch (error) {
      console.error('[Updater] Error checking for updates:', error);
    } finally {
      setIsChecking(false);
    }
  };

  const installUpdate = async () => {
    if (!updateInfo) {
      console.error('[Updater] No update info available');
      return;
    }

    try {
      console.log('[Updater] Downloading and installing update...');
      await updateInfo.downloadAndInstall();

      console.log('[Updater] Update installed, relaunching app...');
      await relaunch();
    } catch (error) {
      console.error('[Updater] Error installing update:', error);
    }
  };

  const dismissUpdate = () => {
    setUpdateAvailable(false);
    setUpdateInfo(null);
  };

  useEffect(() => {
    // Check if we should run an update check
    const shouldCheckForUpdate = () => {
      const lastCheck = localStorage.getItem(LAST_CHECK_KEY);

      if (!lastCheck) {
        return true; // First time, check immediately
      }

      const lastCheckTime = parseInt(lastCheck, 10);
      const timeSinceLastCheck = Date.now() - lastCheckTime;

      return timeSinceLastCheck >= SEVEN_DAYS_MS;
    };

    // Perform initial check on mount if needed
    if (shouldCheckForUpdate()) {
      console.log('[Updater] Performing background update check...');
      checkForUpdate(true);
    }

    // Set up periodic checks every 7 days
    const interval = setInterval(() => {
      if (shouldCheckForUpdate()) {
        console.log('[Updater] Performing scheduled update check...');
        checkForUpdate(true);
      }
    }, SEVEN_DAYS_MS);

    return () => clearInterval(interval);
  }, []);

  return {
    updateAvailable,
    updateInfo,
    isChecking,
    checkForUpdate: () => checkForUpdate(false),
    installUpdate,
    dismissUpdate,
  };
}
