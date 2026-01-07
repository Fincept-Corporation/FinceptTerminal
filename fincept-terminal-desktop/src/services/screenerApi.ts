// Stock Screener API Service
// Tauri command wrappers for stock screener functionality

import { invoke } from '@tauri-apps/api/core';
import type {
  ScreenCriteria,
  ScreenResult,
  SavedScreen,
  MetricInfo,
} from '../types/screener';

export const screenerApi = {
  /**
   * Execute a stock screen with given criteria
   */
  executeScreen: async (criteria: ScreenCriteria): Promise<ScreenResult> => {
    try {
      const response = await invoke<{ success: boolean; data: ScreenResult; error?: string }>(
        'execute_stock_screen',
        { criteria }
      );

      if (!response.success || !response.data) {
        throw new Error(response.error || 'Failed to execute screen');
      }

      return response.data;
    } catch (error) {
      console.error('Error executing screen:', error);
      throw error;
    }
  },

  /**
   * Get preset Value Stocks screen criteria
   */
  getValueScreen: async (): Promise<ScreenCriteria> => {
    try {
      const response = await invoke<{ success: boolean; data: ScreenCriteria; error?: string }>(
        'get_value_screen'
      );

      if (!response.success || !response.data) {
        throw new Error(response.error || 'Failed to get value screen');
      }

      return response.data;
    } catch (error) {
      console.error('Error getting value screen:', error);
      throw error;
    }
  },

  /**
   * Get preset Growth Stocks screen criteria
   */
  getGrowthScreen: async (): Promise<ScreenCriteria> => {
    try {
      const response = await invoke<{ success: boolean; data: ScreenCriteria; error?: string }>(
        'get_growth_screen'
      );

      if (!response.success || !response.data) {
        throw new Error(response.error || 'Failed to get growth screen');
      }

      return response.data;
    } catch (error) {
      console.error('Error getting growth screen:', error);
      throw error;
    }
  },

  /**
   * Get preset Dividend Aristocrats screen criteria
   */
  getDividendScreen: async (): Promise<ScreenCriteria> => {
    try {
      const response = await invoke<{ success: boolean; data: ScreenCriteria; error?: string }>(
        'get_dividend_screen'
      );

      if (!response.success || !response.data) {
        throw new Error(response.error || 'Failed to get dividend screen');
      }

      return response.data;
    } catch (error) {
      console.error('Error getting dividend screen:', error);
      throw error;
    }
  },

  /**
   * Get preset Momentum Stocks screen criteria
   */
  getMomentumScreen: async (): Promise<ScreenCriteria> => {
    try {
      const response = await invoke<{ success: boolean; data: ScreenCriteria; error?: string }>(
        'get_momentum_screen'
      );

      if (!response.success || !response.data) {
        throw new Error(response.error || 'Failed to get momentum screen');
      }

      return response.data;
    } catch (error) {
      console.error('Error getting momentum screen:', error);
      throw error;
    }
  },

  /**
   * Save a custom screen
   */
  saveCustomScreen: async (name: string, criteria: ScreenCriteria): Promise<SavedScreen> => {
    try {
      const response = await invoke<{ success: boolean; data: SavedScreen; error?: string }>(
        'save_custom_screen',
        { name, criteria }
      );

      if (!response.success || !response.data) {
        throw new Error(response.error || 'Failed to save custom screen');
      }

      return response.data;
    } catch (error) {
      console.error('Error saving custom screen:', error);
      throw error;
    }
  },

  /**
   * Load all saved custom screens
   */
  loadCustomScreens: async (): Promise<SavedScreen[]> => {
    try {
      const response = await invoke<{ success: boolean; data: SavedScreen[]; error?: string }>(
        'load_custom_screens'
      );

      if (!response.success) {
        throw new Error(response.error || 'Failed to load custom screens');
      }

      return response.data || [];
    } catch (error) {
      console.error('Error loading custom screens:', error);
      throw error;
    }
  },

  /**
   * Delete a saved custom screen
   */
  deleteCustomScreen: async (screenId: string): Promise<boolean> => {
    try {
      const response = await invoke<{ success: boolean; data: boolean; error?: string }>(
        'delete_custom_screen',
        { screenId }
      );

      if (!response.success) {
        throw new Error(response.error || 'Failed to delete custom screen');
      }

      return response.data || false;
    } catch (error) {
      console.error('Error deleting custom screen:', error);
      throw error;
    }
  },

  /**
   * Get available metrics for screening
   */
  getAvailableMetrics: async (): Promise<MetricInfo[]> => {
    try {
      const response = await invoke<{ success: boolean; data: MetricInfo[]; error?: string }>(
        'get_available_metrics'
      );

      if (!response.success || !response.data) {
        throw new Error(response.error || 'Failed to get available metrics');
      }

      return response.data;
    } catch (error) {
      console.error('Error getting available metrics:', error);
      throw error;
    }
  },
};
