// useTabSession Hook - React hook for tab state persistence
// Automatically saves and restores tab state across navigation

import { useState, useEffect, useCallback, useRef } from 'react';
import { cacheService } from '../services/cache/cacheService';

export interface UseTabSessionOptions<T> {
  tabId: string;
  tabName: string;
  defaultState: T;
  debounceMs?: number; // Debounce save operations (default: 500ms)
  autoSave?: boolean; // Auto-save on state change (default: true)
}

export interface UseTabSessionResult<T> {
  state: T;
  setState: (newState: T | ((prev: T) => T)) => void;
  updateState: (partial: Partial<T>) => void;
  resetState: () => void;
  saveState: () => Promise<void>;
  isRestored: boolean;
  isSaving: boolean;
}

export function useTabSession<T extends object>(
  options: UseTabSessionOptions<T>
): UseTabSessionResult<T> {
  const {
    tabId,
    tabName,
    defaultState,
    debounceMs = 500,
    autoSave = true,
  } = options;

  const [state, setStateInternal] = useState<T>(defaultState);
  const [isRestored, setIsRestored] = useState(false);
  const [isSaving, setIsSaving] = useState(false);

  const mountedRef = useRef(true);
  const saveTimeoutRef = useRef<NodeJS.Timeout | null>(null);
  const stateRef = useRef<T>(state);

  // Keep stateRef in sync
  useEffect(() => {
    stateRef.current = state;
  }, [state]);

  // Save state to SQLite
  const saveState = useCallback(async () => {
    if (!mountedRef.current) return;

    setIsSaving(true);
    try {
      await cacheService.setTabSession(tabId, tabName, stateRef.current);
    } catch (error) {
      console.error(`[useTabSession] Failed to save state for ${tabId}:`, error);
    } finally {
      if (mountedRef.current) {
        setIsSaving(false);
      }
    }
  }, [tabId, tabName]);

  // Debounced save
  const debouncedSave = useCallback(() => {
    if (saveTimeoutRef.current) {
      clearTimeout(saveTimeoutRef.current);
    }
    saveTimeoutRef.current = setTimeout(() => {
      saveState();
    }, debounceMs);
  }, [saveState, debounceMs]);

  // Set state with optional auto-save
  const setState = useCallback(
    (newState: T | ((prev: T) => T)) => {
      setStateInternal((prev) => {
        const next = typeof newState === 'function'
          ? (newState as (prev: T) => T)(prev)
          : newState;
        return next;
      });

      if (autoSave && isRestored) {
        debouncedSave();
      }
    },
    [autoSave, isRestored, debouncedSave]
  );

  // Update partial state
  const updateState = useCallback(
    (partial: Partial<T>) => {
      setState((prev) => ({ ...prev, ...partial }));
    },
    [setState]
  );

  // Reset to default state
  const resetState = useCallback(() => {
    setStateInternal(defaultState);
    if (autoSave) {
      debouncedSave();
    }
  }, [defaultState, autoSave, debouncedSave]);

  // Restore state on mount
  useEffect(() => {
    mountedRef.current = true;

    const restoreState = async () => {
      try {
        const savedState = await cacheService.getTabSession<T>(tabId);
        if (savedState && mountedRef.current) {
          // Merge with default state to handle new fields
          setStateInternal({ ...defaultState, ...savedState });
        }
      } catch (error) {
        console.error(`[useTabSession] Failed to restore state for ${tabId}:`, error);
      } finally {
        if (mountedRef.current) {
          setIsRestored(true);
        }
      }
    };

    restoreState();

    return () => {
      mountedRef.current = false;
      if (saveTimeoutRef.current) {
        clearTimeout(saveTimeoutRef.current);
      }
    };
  }, [tabId]); // Only restore when tabId changes

  // Save on unmount if there's a pending save
  useEffect(() => {
    return () => {
      if (saveTimeoutRef.current) {
        clearTimeout(saveTimeoutRef.current);
        // Synchronous save on unmount is not ideal but ensures data isn't lost
        cacheService.setTabSession(tabId, tabName, stateRef.current).catch(() => {});
      }
    };
  }, [tabId, tabName]);

  return {
    state,
    setState,
    updateState,
    resetState,
    saveState,
    isRestored,
    isSaving,
  };
}

export default useTabSession;
