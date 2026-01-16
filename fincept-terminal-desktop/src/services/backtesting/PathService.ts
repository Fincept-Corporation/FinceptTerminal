/**
 * Path Service
 * Provides OS-specific paths for backtesting data storage
 * Uses Tauri's path API to ensure correct app data directories
 */

import { appDataDir, join } from '@tauri-apps/api/path';

export class PathService {
  private static appDataPath: string | null = null;

  /**
   * Get the base app data directory
   */
  private static async getAppDataDir(): Promise<string> {
    if (!this.appDataPath) {
      this.appDataPath = await appDataDir();
    }
    return this.appDataPath;
  }

  /**
   * Get OS-specific backtesting base directory
   * Windows: C:\Users\<Username>\AppData\Roaming\Fincept\Backtesting
   * macOS: /Users/<Username>/Library/Application Support/Fincept/Backtesting
   * Linux: /home/<username>/.config/fincept/Backtesting
   */
  static async getBacktestingBaseDir(): Promise<string> {
    const appData = await this.getAppDataDir();
    return await join(appData, 'Fincept', 'Backtesting');
  }

  /**
   * Get backtesting projects directory
   */
  static async getProjectsDir(): Promise<string> {
    const baseDir = await this.getBacktestingBaseDir();
    return await join(baseDir, 'projects');
  }

  /**
   * Get backtesting data directory
   */
  static async getDataDir(): Promise<string> {
    const baseDir = await this.getBacktestingBaseDir();
    return await join(baseDir, 'data');
  }

  /**
   * Get backtesting results directory
   */
  static async getResultsDir(): Promise<string> {
    const baseDir = await this.getBacktestingBaseDir();
    return await join(baseDir, 'results');
  }

  /**
   * Get info about current paths configuration
   */
  static async getPathsInfo(): Promise<{
    platform: string;
    baseDir: string;
    projectsDir: string;
    dataDir: string;
    resultsDir: string;
  }> {
    const [baseDir, projectsDir, dataDir, resultsDir] = await Promise.all([
      this.getBacktestingBaseDir(),
      this.getProjectsDir(),
      this.getDataDir(),
      this.getResultsDir(),
    ]);

    return {
      platform: navigator.platform,
      baseDir,
      projectsDir,
      dataDir,
      resultsDir,
    };
  }
}
