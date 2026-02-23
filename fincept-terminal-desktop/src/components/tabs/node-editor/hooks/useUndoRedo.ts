/**
 * useUndoRedo Hook
 *
 * Snapshot-based undo/redo for the Node Editor.
 * Uses useRef for history stacks to avoid re-renders on every snapshot,
 * with useState only for canUndo/canRedo (toolbar button state).
 */

import { useState, useRef, useCallback } from 'react';
import { Node, Edge } from 'reactflow';
import { stripCallbacks } from '../utils';

interface Snapshot {
  nodes: Node[];
  edges: Edge[];
}

export interface UseUndoRedoParams {
  nodes: Node[];
  edges: Edge[];
  setNodes: React.Dispatch<React.SetStateAction<Node[]>>;
  setEdges: React.Dispatch<React.SetStateAction<Edge[]>>;
  isUndoRedoActionRef: React.MutableRefObject<boolean>;
  restoreCallbacks?: (nodes: Node[]) => Node[];
  maxHistory?: number;
  debounceMs?: number;
}

export interface UseUndoRedoReturn {
  takeSnapshot: () => void;
  takeDebouncedSnapshot: () => void;
  undo: () => void;
  redo: () => void;
  canUndo: boolean;
  canRedo: boolean;
}

export function useUndoRedo({
  nodes,
  edges,
  setNodes,
  setEdges,
  isUndoRedoActionRef,
  restoreCallbacks,
  maxHistory = 50,
  debounceMs = 300,
}: UseUndoRedoParams): UseUndoRedoReturn {
  const pastRef = useRef<Snapshot[]>([]);
  const futureRef = useRef<Snapshot[]>([]);
  const debounceTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const pendingSnapshotRef = useRef<boolean>(false);

  const [canUndo, setCanUndo] = useState(false);
  const [canRedo, setCanRedo] = useState(false);

  /** Update the boolean state for toolbar buttons */
  const syncButtonState = useCallback(() => {
    setCanUndo(pastRef.current.length > 0);
    setCanRedo(futureRef.current.length > 0);
  }, []);

  /** Create a serializable snapshot of current state */
  const createSnapshot = useCallback((): Snapshot => {
    return {
      nodes: nodes.map((n) => ({
        ...n,
        data: stripCallbacks(n.data),
      })),
      edges: edges.map((e) => ({ ...e })),
    };
  }, [nodes, edges]);

  /** Push current state to the past stack, clear future */
  const takeSnapshot = useCallback(() => {
    const snapshot = createSnapshot();
    pastRef.current = [...pastRef.current.slice(-(maxHistory - 1)), snapshot];
    futureRef.current = [];
    syncButtonState();
  }, [createSnapshot, maxHistory, syncButtonState]);

  /** Flush any pending debounced snapshot immediately */
  const flushDebouncedSnapshot = useCallback(() => {
    if (debounceTimerRef.current) {
      clearTimeout(debounceTimerRef.current);
      debounceTimerRef.current = null;
    }
    if (pendingSnapshotRef.current) {
      pendingSnapshotRef.current = false;
      // The snapshot was already captured when the debounce started;
      // we just need to ensure it's flushed. Since takeSnapshot captures
      // current state, and the state may have changed, we take a fresh one.
      takeSnapshot();
    }
  }, [takeSnapshot]);

  /** Debounced snapshot for rapid changes (label typing, parameter tweaks) */
  const takeDebouncedSnapshot = useCallback(() => {
    // On first call in a burst, take an immediate snapshot of the pre-change state
    if (!pendingSnapshotRef.current) {
      pendingSnapshotRef.current = true;
      takeSnapshot();
    }

    // Reset the debounce timer
    if (debounceTimerRef.current) {
      clearTimeout(debounceTimerRef.current);
    }
    debounceTimerRef.current = setTimeout(() => {
      pendingSnapshotRef.current = false;
      debounceTimerRef.current = null;
    }, debounceMs);
  }, [takeSnapshot, debounceMs]);

  /** Restore a snapshot */
  const restoreSnapshot = useCallback(
    (snapshot: Snapshot) => {
      isUndoRedoActionRef.current = true;
      const restoredNodes = restoreCallbacks
        ? restoreCallbacks(snapshot.nodes)
        : snapshot.nodes;
      setNodes(restoredNodes);
      setEdges(snapshot.edges);
    },
    [setNodes, setEdges, isUndoRedoActionRef, restoreCallbacks]
  );

  /** Undo: pop from past, push current to future */
  const undo = useCallback(() => {
    flushDebouncedSnapshot();

    if (pastRef.current.length === 0) return;

    const currentSnapshot = createSnapshot();
    const previous = pastRef.current[pastRef.current.length - 1];

    pastRef.current = pastRef.current.slice(0, -1);
    futureRef.current = [...futureRef.current, currentSnapshot];

    restoreSnapshot(previous);
    syncButtonState();
  }, [createSnapshot, restoreSnapshot, flushDebouncedSnapshot, syncButtonState]);

  /** Redo: pop from future, push current to past */
  const redo = useCallback(() => {
    flushDebouncedSnapshot();

    if (futureRef.current.length === 0) return;

    const currentSnapshot = createSnapshot();
    const next = futureRef.current[futureRef.current.length - 1];

    futureRef.current = futureRef.current.slice(0, -1);
    pastRef.current = [...pastRef.current, currentSnapshot];

    restoreSnapshot(next);
    syncButtonState();
  }, [createSnapshot, restoreSnapshot, flushDebouncedSnapshot, syncButtonState]);

  return {
    takeSnapshot,
    takeDebouncedSnapshot,
    undo,
    redo,
    canUndo,
    canRedo,
  };
}
