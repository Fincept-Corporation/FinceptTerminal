/**
 * Lean CLI Executor
 *
 * Handles execution of Lean CLI commands through Tauri.
 * Manages process lifecycle, output parsing, and error handling.
 */

import { invoke } from '@tauri-apps/api/core';
import { LeanCliArgs, LeanConfig, LeanProcessStatus, LeanError } from './types';

/**
 * CLI Command Result
 */
interface CliCommandResult {
  success: boolean;
  stdout?: string;
  stderr?: string;
  exitCode?: number;
  error?: string;
}

/**
 * LeanCliExecutor
 *
 * Executes Lean CLI commands and manages process lifecycle.
 * Uses Tauri commands to bridge to Rust backend which runs actual CLI.
 */
export class LeanCliExecutor {
  private config: LeanConfig;
  private runningProcesses: Map<string, LeanProcessStatus> = new Map();

  constructor(config: LeanConfig) {
    this.config = config;
  }

  /**
   * Run a backtest using Lean CLI
   *
   * @param projectPath Path to Lean project
   * @param args CLI arguments
   * @returns Process ID for tracking
   */
  async runBacktest(
    projectPath: string,
    args: LeanCliArgs
  ): Promise<{ id: string; processId: number }> {
    const backtestId = this.generateId();

    // Build CLI command
    const command = this.buildCliCommand('backtest', projectPath, args);

    // Initialize process status
    this.runningProcesses.set(backtestId, {
      status: 'running',
      progress: 0,
      logs: [],
    });

    try {
      // Execute via Tauri command (Rust backend)
      const result = await invoke<{ process_id: number }>('execute_lean_cli', {
        command,
        workingDir: projectPath,
        backtestId,
      });

      this.runningProcesses.get(backtestId)!.pid = result.process_id;

      return {
        id: backtestId,
        processId: result.process_id,
      };
    } catch (error) {
      this.runningProcesses.get(backtestId)!.status = 'failed';
      this.runningProcesses.get(backtestId)!.error = String(error);
      throw error;
    }
  }

  /**
   * Run optimization using Lean CLI
   */
  async runOptimization(
    projectPath: string,
    args: LeanCliArgs,
    optimizationConfig: any
  ): Promise<{ id: string; processId: number }> {
    const optimizationId = this.generateId();

    const command = this.buildCliCommand('optimize', projectPath, {
      ...args,
      optimizationConfig,
    });

    this.runningProcesses.set(optimizationId, {
      status: 'running',
      progress: 0,
      logs: [],
    });

    try {
      const result = await invoke<{ process_id: number }>('execute_lean_cli', {
        command,
        workingDir: projectPath,
        backtestId: optimizationId,
      });

      this.runningProcesses.get(optimizationId)!.pid = result.process_id;

      return {
        id: optimizationId,
        processId: result.process_id,
      };
    } catch (error) {
      this.runningProcesses.get(optimizationId)!.status = 'failed';
      this.runningProcesses.get(optimizationId)!.error = String(error);
      throw error;
    }
  }

  /**
   * Download historical data using Lean CLI
   */
  async downloadData(
    tickers: string[],
    resolution: string,
    startDate: string,
    endDate: string,
    market: string = 'usa'
  ): Promise<CliCommandResult> {
    const command = [
      this.config.leanCliPath,
      'data',
      'download',
      '--tickers',
      tickers.join(','),
      '--resolution',
      resolution,
      '--start',
      startDate,
      '--end',
      endDate,
      '--market',
      market,
      '--data-folder',
      this.config.dataPath,
    ].join(' ');

    return this.executeCommand(command);
  }

  /**
   * Create a new Lean project
   */
  async createProject(
    projectName: string,
    language: 'python' = 'python'
  ): Promise<CliCommandResult> {
    const command = [
      this.config.leanCliPath,
      'project',
      'create',
      '--name',
      projectName,
      '--language',
      language,
      '--path',
      this.config.projectsPath,
    ].join(' ');

    return this.executeCommand(command);
  }

  /**
   * Get status of a running process
   */
  getProcessStatus(backtestId: string): LeanProcessStatus | null {
    return this.runningProcesses.get(backtestId) || null;
  }

  /**
   * Cancel a running backtest
   */
  async cancelBacktest(backtestId: string): Promise<void> {
    const process = this.runningProcesses.get(backtestId);
    if (!process || !process.pid) {
      throw new Error(`Process ${backtestId} not found or not running`);
    }

    try {
      await invoke('kill_lean_process', {
        processId: process.pid,
      });

      process.status = 'completed';  // Mark as stopped
      this.runningProcesses.delete(backtestId);
    } catch (error) {
      throw new Error(`Failed to cancel backtest: ${error}`);
    }
  }

  /**
   * Update process status (called by Rust backend via events)
   */
  updateProcessStatus(backtestId: string, update: Partial<LeanProcessStatus>): void {
    const existing = this.runningProcesses.get(backtestId);
    if (existing) {
      this.runningProcesses.set(backtestId, {
        ...existing,
        ...update,
      });
    }
  }

  /**
   * Append log to process
   */
  appendLog(backtestId: string, log: string): void {
    const process = this.runningProcesses.get(backtestId);
    if (process) {
      process.logs.push(log);
    }
  }

  /**
   * Build CLI command from arguments
   */
  private buildCliCommand(
    mode: 'backtest' | 'optimize' | 'live' | 'research',
    projectPath: string,
    args: LeanCliArgs
  ): string {
    const commandParts = [this.config.leanCliPath];

    // Mode
    commandParts.push(mode);

    // Project
    commandParts.push('--project', projectPath);

    // Algorithm
    if (args.algorithm) {
      commandParts.push('--algorithm-file', args.algorithm);
    }

    // Dates
    if (args.startDate) {
      commandParts.push('--start', args.startDate);
    }
    if (args.endDate) {
      commandParts.push('--end', args.endDate);
    }

    // Capital
    if (args.cash) {
      commandParts.push('--cash', args.cash.toString());
    }

    // Data folder
    if (args.dataFolder || this.config.dataPath) {
      commandParts.push('--data-folder', args.dataFolder || this.config.dataPath);
    }

    // Results folder
    if (args.resultsFolder || this.config.resultsPath) {
      commandParts.push('--results', args.resultsFolder || this.config.resultsPath);
    }

    // Parameters (as JSON)
    if (args.parameters && Object.keys(args.parameters).length > 0) {
      commandParts.push('--parameters', JSON.stringify(args.parameters));
    }

    // Environment
    if (args.environment) {
      commandParts.push('--environment', args.environment);
    }

    // Flags
    if (args.release) {
      commandParts.push('--release');
    }
    if (args.update) {
      commandParts.push('--update');
    }

    return commandParts.join(' ');
  }

  /**
   * Execute a generic CLI command
   */
  private async executeCommand(command: string): Promise<CliCommandResult> {
    try {
      const result = await invoke<{
        success: boolean;
        stdout: string;
        stderr: string;
        exit_code: number;
      }>('execute_command', { command });

      return {
        success: result.success,
        stdout: result.stdout,
        stderr: result.stderr,
        exitCode: result.exit_code,
      };
    } catch (error) {
      return {
        success: false,
        error: String(error),
      };
    }
  }

  /**
   * Generate unique ID
   */
  private generateId(): string {
    return `lean_${Date.now()}_${Math.random().toString(36).substring(2, 9)}`;
  }

  /**
   * Clean up completed processes (optional)
   */
  cleanupCompletedProcesses(): void {
    for (const [id, process] of this.runningProcesses.entries()) {
      if (process.status === 'completed' || process.status === 'failed') {
        this.runningProcesses.delete(id);
      }
    }
  }

  /**
   * Get all running processes
   */
  getRunningProcesses(): Map<string, LeanProcessStatus> {
    return new Map(this.runningProcesses);
  }
}
