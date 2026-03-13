// File: src/components/auth/SessionGuard.tsx
// Polls /user/validate-session every 30s. On 401 → instant logout.
// Pauses when tab is hidden.

import { useEffect, useRef, useCallback } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { AuthApiService } from '@/services/auth/authApi';

const PULSE_INTERVAL_MS = 30_000; // 30 seconds

export const SessionGuard: React.FC = () => {
  const { session, logout } = useAuth();
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const isCheckingRef = useRef(false);
  const logoutRef = useRef(logout);
  logoutRef.current = logout;

  const clearPoll = useCallback(() => {
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
      intervalRef.current = null;
    }
  }, []);

  const checkPulse = useCallback(async () => {
    if (!session?.api_key || !session.session_token || session.user_type !== 'registered') return;
    if (isCheckingRef.current) return;

    isCheckingRef.current = true;
    try {
      const result = await AuthApiService.validateSessionServer(session.api_key);

      // 401 = session expired / logged in from another device
      if (!result.success && (result.status_code === 401 || result.status_code === 403)) {
        console.warn('[SessionGuard] Session expired (status', result.status_code, ') — logging out');
        clearPoll();
        await logoutRef.current();
        return;
      }

      // Also check the response body for valid: false
      const data = (result.data as any)?.data || result.data;
      if (result.success && data && data.valid === false) {
        console.warn('[SessionGuard] Session invalid (valid=false) — logging out');
        clearPoll();
        await logoutRef.current();
      }
    } catch {
      // Network errors are non-fatal — don't logout on connectivity issues
    } finally {
      isCheckingRef.current = false;
    }
  }, [session?.api_key, session?.session_token, session?.user_type, clearPoll]);

  // Start/stop polling based on session state
  useEffect(() => {
    if (session?.user_type === 'registered' && session.session_token) {
      checkPulse();
      clearPoll();
      intervalRef.current = setInterval(checkPulse, PULSE_INTERVAL_MS);
    } else {
      clearPoll();
    }

    return clearPoll;
  }, [session?.user_type, session?.session_token, checkPulse, clearPoll]);

  // Pause when tab is hidden, resume when visible
  useEffect(() => {
    const handleVisibility = () => {
      if (document.hidden) {
        clearPoll();
      } else if (session?.user_type === 'registered' && session.session_token) {
        checkPulse();
        clearPoll();
        intervalRef.current = setInterval(checkPulse, PULSE_INTERVAL_MS);
      }
    };

    document.addEventListener('visibilitychange', handleVisibility);
    return () => document.removeEventListener('visibilitychange', handleVisibility);
  }, [session?.user_type, session?.session_token, checkPulse, clearPoll]);

  return null;
};
