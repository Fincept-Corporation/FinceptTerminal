import { useEffect, useState } from 'react';
import { check, Update } from '@tauri-apps/plugin-updater';
import { relaunch } from '@tauri-apps/plugin-process';
import { Button } from '@/components/ui/button';
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog';
import { Progress } from '@/components/ui/progress';
import { Alert, AlertDescription, AlertTitle } from '@/components/ui/alert';
import { Download, RefreshCw, CheckCircle2, XCircle, Info } from 'lucide-react';

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

  const checkForUpdates = async (silent = false) => {
    try {
      setChecking(true);
      setError(null);

      console.log('[AutoUpdater] Checking for updates...');
      const update = await check();

      if (update?.available) {
        console.log(`[AutoUpdater] Update available: v${update.version}`);
        console.log(`[AutoUpdater] Current version: v${update.currentVersion}`);
        console.log(`[AutoUpdater] Release date: ${update.date}`);

        setUpdateInfo(update);
        setUpdateAvailable(true);

        if (!silent) {
          setShowDialog(true);
        }
      } else {
        console.log('[AutoUpdater] No updates available');

        if (!silent) {
          // Show a brief notification that app is up to date
          setError('You are running the latest version!');
          setTimeout(() => setError(null), 3000);
        }
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Unknown error occurred';
      console.error('[AutoUpdater] Error checking for updates:', errorMessage);

      // Only show error if it's not a silent check and not a network error
      if (!silent && !errorMessage.includes('network')) {
        setError(`Failed to check for updates: ${errorMessage}`);
      }
    } finally {
      setChecking(false);
    }
  };

  const downloadAndInstall = async () => {
    if (!updateInfo) return;

    try {
      setDownloading(true);
      setError(null);
      setDownloadProgress(0);

      console.log('[AutoUpdater] Starting download and installation...');

      // Download and install with progress tracking
      let totalBytes = 0;
      let downloadedBytes = 0;

      await updateInfo.downloadAndInstall((event) => {
        switch (event.event) {
          case 'Started':
            totalBytes = (event.data as any).contentLength || 0;
            downloadedBytes = 0;
            console.log(`[AutoUpdater] Download started - Total size: ${totalBytes} bytes`);
            setDownloadProgress(0);
            break;
          case 'Progress':
            downloadedBytes += event.data.chunkLength;
            const progress = totalBytes > 0 ? (downloadedBytes / totalBytes) * 100 : 0;
            console.log(`[AutoUpdater] Download progress: ${progress.toFixed(2)}%`);
            setDownloadProgress(progress);
            break;
          case 'Finished':
            console.log('[AutoUpdater] Download finished, installing...');
            setDownloadProgress(100);
            break;
        }
      });

      console.log('[AutoUpdater] Update installed successfully! Preparing to relaunch...');
      setUpdateReady(true);

      // Auto-relaunch after 2 seconds
      setTimeout(async () => {
        console.log('[AutoUpdater] Relaunching application...');
        await relaunch();
      }, 2000);

    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Unknown error occurred';
      console.error('[AutoUpdater] Error during download/install:', errorMessage);
      setError(`Failed to install update: ${errorMessage}`);
      setDownloading(false);
    }
  };

  const handleLater = () => {
    setShowDialog(false);
    console.log('[AutoUpdater] Update postponed by user');
  };

  // Check for updates on mount
  useEffect(() => {
    if (checkOnMount) {
      // Delay initial check by 3 seconds to not interfere with app startup
      const timer = setTimeout(() => {
        checkForUpdates(true);
      }, 3000);

      return () => clearTimeout(timer);
    }
  }, [checkOnMount]);

  // Set up periodic update checks
  useEffect(() => {
    if (checkIntervalMinutes > 0) {
      const interval = setInterval(() => {
        checkForUpdates(true);
      }, checkIntervalMinutes * 60 * 1000);

      return () => clearInterval(interval);
    }
  }, [checkIntervalMinutes]);

  return (
    <>
      {/* Update Dialog */}
      <Dialog open={showDialog} onOpenChange={setShowDialog}>
        <DialogContent className="sm:max-w-[500px]">
          <DialogHeader>
            <DialogTitle className="flex items-center gap-2">
              <Download className="h-5 w-5" />
              Update Available
            </DialogTitle>
            <DialogDescription>
              A new version of FinceptTerminal is available.
            </DialogDescription>
          </DialogHeader>

          <div className="space-y-4 py-4">
            {updateInfo && (
              <div className="space-y-2">
                <div className="flex justify-between text-sm">
                  <span className="text-muted-foreground">Current Version:</span>
                  <span className="font-mono">{updateInfo.currentVersion}</span>
                </div>
                <div className="flex justify-between text-sm">
                  <span className="text-muted-foreground">New Version:</span>
                  <span className="font-mono font-bold text-green-600">{updateInfo.version}</span>
                </div>
                <div className="flex justify-between text-sm">
                  <span className="text-muted-foreground">Release Date:</span>
                  <span>{updateInfo.date ? new Date(updateInfo.date).toLocaleDateString() : 'N/A'}</span>
                </div>
              </div>
            )}

            {updateInfo?.body && (
              <div className="rounded-md bg-muted p-3 text-sm">
                <p className="font-semibold mb-2">Release Notes:</p>
                <p className="text-muted-foreground whitespace-pre-wrap">{updateInfo.body}</p>
              </div>
            )}

            {downloading && (
              <div className="space-y-2">
                <div className="flex items-center justify-between text-sm">
                  <span>Downloading update...</span>
                  <span>{downloadProgress.toFixed(0)}%</span>
                </div>
                <Progress value={downloadProgress} />
              </div>
            )}

            {updateReady && (
              <Alert className="border-green-500 bg-green-50 dark:bg-green-950">
                <CheckCircle2 className="h-4 w-4 text-green-600" />
                <AlertTitle>Update Ready!</AlertTitle>
                <AlertDescription>
                  The application will restart in a moment...
                </AlertDescription>
              </Alert>
            )}

            {error && (
              <Alert variant="destructive">
                <XCircle className="h-4 w-4" />
                <AlertTitle>Error</AlertTitle>
                <AlertDescription>{error}</AlertDescription>
              </Alert>
            )}
          </div>

          <DialogFooter>
            {!downloading && !updateReady && (
              <>
                <Button variant="outline" onClick={handleLater}>
                  Later
                </Button>
                <Button onClick={downloadAndInstall} disabled={downloading}>
                  <Download className="mr-2 h-4 w-4" />
                  Download & Install
                </Button>
              </>
            )}
            {downloading && (
              <Button disabled>
                <RefreshCw className="mr-2 h-4 w-4 animate-spin" />
                Installing...
              </Button>
            )}
          </DialogFooter>
        </DialogContent>
      </Dialog>

      {/* Manual Check Button (can be placed in settings/menu) */}
      <Button
        variant="ghost"
        size="sm"
        onClick={() => checkForUpdates(false)}
        disabled={checking || downloading}
        className="gap-2"
      >
        {checking ? (
          <>
            <RefreshCw className="h-4 w-4 animate-spin" />
            Checking...
          </>
        ) : (
          <>
            <Info className="h-4 w-4" />
            Check for Updates
          </>
        )}
      </Button>

      {/* Success/Error Toast */}
      {error && !showDialog && (
        <div className="fixed bottom-4 right-4 z-50 max-w-md">
          <Alert className={error.includes('latest version') ? 'border-green-500' : 'border-red-500'}>
            {error.includes('latest version') ? (
              <CheckCircle2 className="h-4 w-4 text-green-600" />
            ) : (
              <XCircle className="h-4 w-4" />
            )}
            <AlertDescription>{error}</AlertDescription>
          </Alert>
        </div>
      )}
    </>
  );
}
