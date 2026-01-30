/**
 * Stock Broker Auth State Machine
 *
 * Atomic state transitions for broker authentication.
 * Prevents race conditions by bundling related state updates.
 */

import type { IStockBrokerAdapter } from '@/brokers/stocks/types';

// ============================================================================
// STATE TYPES
// ============================================================================

export type AuthStatus =
  | 'idle'           // No broker selected
  | 'initializing'   // Loading credentials from DB
  | 'connecting'     // Attempting to authenticate
  | 'authenticated'  // Successfully connected
  | 'error'          // Authentication failed
  | 'disconnecting'; // Logging out

export interface AuthState {
  status: AuthStatus;
  brokerId: string | null;
  adapter: IStockBrokerAdapter | null;
  error: string | null;
  timestamp: number;
}

// ============================================================================
// ACTIONS
// ============================================================================

export type AuthAction =
  | { type: 'SELECT_BROKER'; brokerId: string }
  | { type: 'INIT_START' }
  | { type: 'INIT_SUCCESS'; adapter: IStockBrokerAdapter }
  | { type: 'INIT_FAILURE'; error: string }
  | { type: 'CONNECT_START' }
  | { type: 'CONNECT_SUCCESS'; adapter: IStockBrokerAdapter }
  | { type: 'CONNECT_FAILURE'; error: string }
  | { type: 'DISCONNECT_START' }
  | { type: 'DISCONNECT_COMPLETE' }
  | { type: 'CLEAR_ERROR' }
  | { type: 'RESET' };

// ============================================================================
// INITIAL STATE
// ============================================================================

export const initialAuthState: AuthState = {
  status: 'idle',
  brokerId: null,
  adapter: null,
  error: null,
  timestamp: 0,
};

// ============================================================================
// REDUCER
// ============================================================================

export function authReducer(state: AuthState, action: AuthAction): AuthState {
  const timestamp = Date.now();

  switch (action.type) {
    case 'SELECT_BROKER':
      return {
        ...state,
        brokerId: action.brokerId,
        status: 'idle',
        adapter: null,
        error: null,
        timestamp,
      };

    case 'INIT_START':
      return {
        ...state,
        status: 'initializing',
        error: null,
        timestamp,
      };

    case 'INIT_SUCCESS':
      return {
        ...state,
        status: 'authenticated',
        adapter: action.adapter,
        error: null,
        timestamp,
      };

    case 'INIT_FAILURE':
      return {
        ...state,
        status: 'error',
        adapter: null,
        error: action.error,
        timestamp,
      };

    case 'CONNECT_START':
      return {
        ...state,
        status: 'connecting',
        error: null,
        timestamp,
      };

    case 'CONNECT_SUCCESS':
      return {
        ...state,
        status: 'authenticated',
        adapter: action.adapter,
        error: null,
        timestamp,
      };

    case 'CONNECT_FAILURE':
      return {
        ...state,
        status: 'error',
        error: action.error,
        timestamp,
      };

    case 'DISCONNECT_START':
      return {
        ...state,
        status: 'disconnecting',
        timestamp,
      };

    case 'DISCONNECT_COMPLETE':
      return {
        ...state,
        status: 'idle',
        adapter: null,
        error: null,
        timestamp,
      };

    case 'CLEAR_ERROR':
      return {
        ...state,
        error: null,
        status: state.adapter ? 'authenticated' : 'idle',
        timestamp,
      };

    case 'RESET':
      return {
        ...initialAuthState,
        timestamp,
      };

    default:
      return state;
  }
}

// ============================================================================
// SELECTORS
// ============================================================================

export const authSelectors = {
  isAuthenticated: (state: AuthState): boolean =>
    state.status === 'authenticated' && state.adapter !== null,

  isLoading: (state: AuthState): boolean =>
    state.status === 'initializing' || state.status === 'connecting',

  isIdle: (state: AuthState): boolean =>
    state.status === 'idle',

  hasError: (state: AuthState): boolean =>
    state.status === 'error' && state.error !== null,

  canConnect: (state: AuthState): boolean =>
    state.brokerId !== null && (state.status === 'idle' || state.status === 'error'),
};

// ============================================================================
// STATE MACHINE VALIDATION
// ============================================================================

const validTransitions: Record<AuthStatus, AuthAction['type'][]> = {
  idle: ['SELECT_BROKER', 'INIT_START', 'CONNECT_START', 'RESET'],
  initializing: ['INIT_SUCCESS', 'INIT_FAILURE', 'RESET'],
  connecting: ['CONNECT_SUCCESS', 'CONNECT_FAILURE', 'RESET'],
  authenticated: ['DISCONNECT_START', 'SELECT_BROKER', 'RESET'],
  error: ['CONNECT_START', 'CLEAR_ERROR', 'SELECT_BROKER', 'RESET'],
  disconnecting: ['DISCONNECT_COMPLETE', 'RESET'],
};

export function isValidTransition(state: AuthState, action: AuthAction): boolean {
  const allowedActions = validTransitions[state.status];
  return allowedActions?.includes(action.type) ?? false;
}
