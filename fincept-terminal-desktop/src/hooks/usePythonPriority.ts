/**
 * Hook for priority-aware Python script execution
 * Automatically determines priority based on active tab state
 */

import { useNavigation } from '@/contexts/NavigationContext';
import { invoke } from '@tauri-apps/api/core';

export function usePythonPriority(currentTabId: string) {
  const { activeTab } = useNavigation();

  // Check if this tab is currently active
  const isActiveTab = activeTab === currentTabId;

  /**
   * Execute Python command with automatic priority detection
   * @param command - Tauri command name
   * @param params - Command parameters
   * @returns Promise with command result
   */
  const executePython = async <T = any>(
    command: string,
    params: Record<string, any> = {}
  ): Promise<T> => {
    return invoke<T>(command, {
      ...params,
      is_active_tab: isActiveTab,
    });
  };

  /**
   * Execute Python command with explicit priority
   * @param command - Tauri command name
   * @param params - Command parameters
   * @param priority - Explicit priority (true=HIGH, false=NORMAL)
   * @returns Promise with command result
   */
  const executePythonWithPriority = async <T = any>(
    command: string,
    params: Record<string, any>,
    priority: boolean
  ): Promise<T> => {
    return invoke<T>(command, {
      ...params,
      is_active_tab: priority,
    });
  };

  /**
   * Execute background task (always LOW priority)
   * @param command - Tauri command name
   * @param params - Command parameters
   * @returns Promise with command result
   */
  const executeBackground = async <T = any>(
    command: string,
    params: Record<string, any> = {}
  ): Promise<T> => {
    return invoke<T>(command, {
      ...params,
      is_active_tab: false,
    });
  };

  return {
    executePython,
    executePythonWithPriority,
    executeBackground,
    isActiveTab,
  };
}
