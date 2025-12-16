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
   * Get OS-specific Lean base directory
   * Windows: C:\Users\<Username>\AppData\Roaming\Fincept\Lean
   * macOS: /Users/<Username>/Library/Application Support/Fincept/Lean
   * Linux: /home/<username>/.config/fincept/Lean
   */
  static async getLeanBaseDir(): Promise<string> {
    const appData = await this.getAppDataDir();
    return await join(appData, 'Fincept', 'Lean');
  }

  /**
   * Get Lean projects directory
   */
  static async getLeanProjectsDir(): Promise<string> {
    const baseDir = await this.getLeanBaseDir();
    return await join(baseDir, 'projects');
  }

  /**
   * Get Lean data directory
   */
  static async getLeanDataDir(): Promise<string> {
    const baseDir = await this.getLeanBaseDir();
    return await join(baseDir, 'data');
  }

  /**
   * Get Lean results directory
   */
  static async getLeanResultsDir(): Promise<string> {
    const baseDir = await this.getLeanBaseDir();
    return await join(baseDir, 'results');
  }

  /**
   * Get default Lean configuration with OS-specific paths
   */
  static async getDefaultLeanConfig(): Promise<{
    leanCliPath: string;
    projectsPath: string;
    dataPath: string;
    resultsPath: string;
    environment: string;
  }> {
    const [projectsPath, dataPath, resultsPath] = await Promise.all([
      this.getLeanProjectsDir(),
      this.getLeanDataDir(),
      this.getLeanResultsDir(),
    ]);

    // Determine default CLI path based on OS
    let leanCliPath = 'lean'; // Default to PATH

    // Check platform
    if (navigator.platform.toLowerCase().includes('win')) {
      // Windows: Try common Python Scripts location
      const username = await this.getUsername();
      leanCliPath = `C:/Users/${username}/AppData/Roaming/Python/Python313/Scripts/lean.exe`;
    } else if (navigator.platform.toLowerCase().includes('mac')) {
      // macOS: Try homebrew or pip location
      leanCliPath = '/usr/local/bin/lean';
    } else {
      // Linux: Try common locations
      leanCliPath = '/usr/local/bin/lean';
    }

    return {
      leanCliPath,
      projectsPath,
      dataPath,
      resultsPath,
      environment: 'backtesting',
    };
  }

  /**
   * Get current username (for default path construction)
   */
  private static async getUsername(): Promise<string> {
    try {
      // Try to extract from app data path
      const appData = await this.getAppDataDir();
      const parts = appData.split(/[\\/]/);

      // On Windows, path is like C:\Users\<Username>\AppData\Roaming
      // On macOS, path is like /Users/<Username>/Library/Application Support
      // On Linux, path is like /home/<username>/.config

      if (navigator.platform.toLowerCase().includes('win')) {
        const userIndex = parts.indexOf('Users');
        if (userIndex >= 0 && userIndex + 1 < parts.length) {
          return parts[userIndex + 1];
        }
      } else {
        const userIndex = parts.indexOf('Users') >= 0 ? parts.indexOf('Users') : parts.indexOf('home');
        if (userIndex >= 0 && userIndex + 1 < parts.length) {
          return parts[userIndex + 1];
        }
      }
    } catch (error) {
      console.error('[PathService] Failed to extract username:', error);
    }

    return 'User'; // Fallback
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
      this.getLeanBaseDir(),
      this.getLeanProjectsDir(),
      this.getLeanDataDir(),
      this.getLeanResultsDir(),
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
