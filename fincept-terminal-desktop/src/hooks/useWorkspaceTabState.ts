import { useEffect, useRef } from 'react';
import { useWorkspace } from '@/contexts/WorkspaceContext';

/**
 * Hook for registering a tab's state with the workspace system.
 *
 * On mount: registers getState/setState with the workspace context
 *           and checks for pending restored state.
 * On unmount: unregisters the tab.
 *
 * @param tabId - Unique identifier for this tab (e.g. 'screener', 'chat')
 * @param getState - Callback returning the tab's current saveable state
 * @param setState - Callback to apply restored state to the tab
 */
export function useWorkspaceTabState(
  tabId: string,
  getState: () => Record<string, unknown>,
  setState: (state: Record<string, unknown>) => void,
): void {
  const { registerTabState, unregisterTabState } = useWorkspace();
  const restoredRef = useRef(false);

  useEffect(() => {
    // Wrap setState to prevent double-restoration
    const guardedSetter = (state: Record<string, unknown>) => {
      if (!restoredRef.current) {
        restoredRef.current = true;
        setState(state);
      }
    };

    registerTabState(tabId, getState, guardedSetter);

    return () => {
      unregisterTabState(tabId);
      restoredRef.current = false;
    };
  }, [tabId, getState, setState, registerTabState, unregisterTabState]);
}
