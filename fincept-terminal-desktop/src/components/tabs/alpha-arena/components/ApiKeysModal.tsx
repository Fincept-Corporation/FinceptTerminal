/**
 * API Keys Modal Component
 *
 * @deprecated This component is no longer used. API keys are now managed
 * exclusively through Settings > LLM Config. Kept for reference only.
 */

import React, { useState, useEffect, useReducer, useRef } from 'react';
import { X, Key, Eye, EyeOff, Save, Loader2, AlertCircle } from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';

interface ApiKeysModalProps {
  isOpen: boolean;
  onClose: () => void;
}

interface ApiKeyConfig {
  provider: string;
  label: string;
  envVar: string;
  value: string;
  placeholder: string;
}

const DEFAULT_PROVIDERS: Omit<ApiKeyConfig, 'value'>[] = [
  {
    provider: 'openai',
    label: 'OpenAI',
    envVar: 'OPENAI_API_KEY',
    placeholder: 'sk-...',
  },
  {
    provider: 'anthropic',
    label: 'Anthropic',
    envVar: 'ANTHROPIC_API_KEY',
    placeholder: 'sk-ant-...',
  },
  {
    provider: 'google',
    label: 'Google AI',
    envVar: 'GOOGLE_API_KEY',
    placeholder: 'AIza...',
  },
  {
    provider: 'deepseek',
    label: 'DeepSeek',
    envVar: 'DEEPSEEK_API_KEY',
    placeholder: 'sk-...',
  },
  {
    provider: 'groq',
    label: 'Groq',
    envVar: 'GROQ_API_KEY',
    placeholder: 'gsk_...',
  },
];

// State machine for load/save operations
type LoadState =
  | { status: 'idle' }
  | { status: 'loading' }
  | { status: 'success' }
  | { status: 'error'; error: string };

type SaveState =
  | { status: 'idle' }
  | { status: 'saving' }
  | { status: 'saved' }
  | { status: 'error'; error: string };

type LoadAction =
  | { type: 'LOAD_START' }
  | { type: 'LOAD_SUCCESS' }
  | { type: 'LOAD_ERROR'; error: string };

type SaveAction =
  | { type: 'SAVE_START' }
  | { type: 'SAVE_SUCCESS' }
  | { type: 'SAVE_ERROR'; error: string }
  | { type: 'SAVE_RESET' };

function loadReducer(_state: LoadState, action: LoadAction): LoadState {
  switch (action.type) {
    case 'LOAD_START': return { status: 'loading' };
    case 'LOAD_SUCCESS': return { status: 'success' };
    case 'LOAD_ERROR': return { status: 'error', error: action.error };
    default: return _state;
  }
}

function saveReducer(_state: SaveState, action: SaveAction): SaveState {
  switch (action.type) {
    case 'SAVE_START': return { status: 'saving' };
    case 'SAVE_SUCCESS': return { status: 'saved' };
    case 'SAVE_ERROR': return { status: 'error', error: action.error };
    case 'SAVE_RESET': return { status: 'idle' };
    default: return _state;
  }
}

const ApiKeysModal: React.FC<ApiKeysModalProps> = ({ isOpen, onClose }) => {
  const [apiKeys, setApiKeys] = useState<ApiKeyConfig[]>(
    DEFAULT_PROVIDERS.map((p) => ({ ...p, value: '' }))
  );
  const [visibleKeys, setVisibleKeys] = useState<Record<string, boolean>>({});
  const [loadState, dispatchLoad] = useReducer(loadReducer, { status: 'loading' });
  const [saveState, dispatchSave] = useReducer(saveReducer, { status: 'idle' });
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  // Load existing keys on mount
  useEffect(() => {
    if (isOpen) {
      loadApiKeys();
    }
  }, [isOpen]);

  const loadApiKeys = async () => {
    dispatchLoad({ type: 'LOAD_START' });
    // API keys are now managed in Settings > LLM Config
    dispatchLoad({ type: 'LOAD_SUCCESS' });
  };

  const handleKeyChange = (provider: string, value: string) => {
    setApiKeys((prev) =>
      prev.map((key) => (key.provider === provider ? { ...key, value } : key))
    );
    if (saveState.status === 'saved') {
      dispatchSave({ type: 'SAVE_RESET' });
    }
  };

  const toggleVisibility = (provider: string) => {
    setVisibleKeys((prev) => ({
      ...prev,
      [provider]: !prev[provider],
    }));
  };

  const handleSave = async () => {
    // API keys are now managed in Settings > LLM Config
    dispatchSave({ type: 'SAVE_ERROR', error: 'Please configure API keys in Settings > LLM Config instead.' });
  };

  const isLoading = loadState.status === 'loading';
  const isSaving = saveState.status === 'saving';
  const error = loadState.status === 'error' ? loadState.error : saveState.status === 'error' ? saveState.error : null;
  const success = saveState.status === 'saved';

  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 bg-black/80 flex items-center justify-center z-50">
      <div className="bg-[#0F0F0F] rounded-lg border border-[#2A2A2A] w-full max-w-lg">
        {/* Header */}
        <div className="flex items-center justify-between px-6 py-4 border-b border-[#2A2A2A]">
          <div className="flex items-center gap-2">
            <Key className="w-5 h-5 text-[#FF8800]" />
            <h2 className="text-lg font-semibold text-white">API Keys</h2>
          </div>
          <button
            onClick={onClose}
            className="p-1 rounded hover:bg-[#2A2A2A] text-[#787878] hover:text-white"
          >
            <X className="w-5 h-5" />
          </button>
        </div>

        {/* Content */}
        <div className="p-6 space-y-4">
          {error && (
            <div className="flex items-center gap-2 p-3 bg-red-500/10 border border-red-500/30 rounded-lg text-red-400 text-sm">
              <AlertCircle className="w-4 h-4 flex-shrink-0" />
              {error}
            </div>
          )}

          {success && (
            <div className="p-3 bg-green-500/10 border border-green-500/30 rounded-lg text-green-400 text-sm">
              API keys saved successfully
            </div>
          )}

          <p className="text-sm text-[#787878]">
            Configure API keys for LLM providers. Keys are stored securely and
            used for AI trading agents.
          </p>

          {isLoading ? (
            <div className="flex items-center justify-center py-8">
              <Loader2 className="w-6 h-6 animate-spin text-[#FF8800]" />
            </div>
          ) : (
            <div className="space-y-3">
              {apiKeys.map((key) => (
                <div key={key.provider}>
                  <label className="block text-sm font-medium text-white mb-1">
                    {key.label}
                  </label>
                  <div className="relative">
                    <input
                      type={visibleKeys[key.provider] ? 'text' : 'password'}
                      value={key.value}
                      onChange={(e) =>
                        handleKeyChange(key.provider, e.target.value)
                      }
                      placeholder={key.placeholder}
                      className="w-full px-3 py-2 pr-10 bg-[#1A1A1A] border border-[#2A2A2A] rounded-lg text-white placeholder-[#505050] focus:border-[#FF8800] focus:outline-none font-mono text-sm"
                    />
                    <button
                      type="button"
                      onClick={() => toggleVisibility(key.provider)}
                      className="absolute right-2 top-1/2 -translate-y-1/2 p-1 text-[#787878] hover:text-white"
                    >
                      {visibleKeys[key.provider] ? (
                        <EyeOff className="w-4 h-4" />
                      ) : (
                        <Eye className="w-4 h-4" />
                      )}
                    </button>
                  </div>
                  <p className="text-xs text-[#505050] mt-1">{key.envVar}</p>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="flex justify-end gap-3 px-6 py-4 border-t border-[#2A2A2A]">
          <button
            onClick={onClose}
            className="px-4 py-2 rounded-lg border border-[#2A2A2A] text-[#787878] hover:bg-[#1A1A1A]"
          >
            Cancel
          </button>
          <button
            onClick={handleSave}
            disabled={isSaving}
            className="px-4 py-2 rounded-lg bg-[#FF8800] text-black font-medium hover:bg-[#FF9900] disabled:opacity-50 flex items-center gap-2"
          >
            {isSaving ? (
              <>
                <Loader2 className="w-4 h-4 animate-spin" />
                Saving...
              </>
            ) : (
              <>
                <Save className="w-4 h-4" />
                Save Keys
              </>
            )}
          </button>
        </div>
      </div>
    </div>
  );
};

export default withErrorBoundary(ApiKeysModal, { name: 'AlphaArena.ApiKeysModal' });
