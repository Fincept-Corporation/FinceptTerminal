import js from '@eslint/js';
import tseslint from 'typescript-eslint';
import reactHooks from 'eslint-plugin-react-hooks';
import reactRefresh from 'eslint-plugin-react-refresh';

export default tseslint.config(
  // Ignore generated/vendored files
  {
    ignores: [
      'dist/**',
      'node_modules/**',
      'src-tauri/**',
      'public/**',
      'scripts/**',
      '*.config.*',
    ],
  },

  // Base JS recommended rules
  js.configs.recommended,

  // TypeScript rules
  ...tseslint.configs.recommended,

  // React-specific rules
  {
    files: ['src/**/*.{ts,tsx}'],
    plugins: {
      'react-hooks': reactHooks,
      'react-refresh': reactRefresh,
    },
    rules: {
      // ── React Hooks ──────────────────────────────────────────────────────
      // Catches missing useEffect deps, conditional hooks — root cause of
      // stale closure bugs and render loops
      'react-hooks/rules-of-hooks': 'error',
      'react-hooks/exhaustive-deps': 'warn',

      // ── React Refresh (HMR safety) ───────────────────────────────────────
      'react-refresh/only-export-components': ['warn', { allowConstantExport: true }],

      // ── TypeScript `any` — root cause of object-rendered-as-JSX bugs ────
      // Warn (not error) so the codebase isn't blocked, but issues surface
      '@typescript-eslint/no-explicit-any': 'warn',

      // Catches cases where `any` flows in from untyped sources silently
      '@typescript-eslint/no-unsafe-assignment': 'off',   // too noisy for now
      '@typescript-eslint/no-unsafe-member-access': 'off', // too noisy for now
      '@typescript-eslint/no-unsafe-call': 'off',          // too noisy for now
      '@typescript-eslint/no-unsafe-return': 'off',        // too noisy for now

      // ── Unused vars — catches dead code that hides real issues ───────────
      '@typescript-eslint/no-unused-vars': ['warn', {
        argsIgnorePattern: '^_',
        varsIgnorePattern: '^_',
        ignoreRestSiblings: true,
      }],

      // ── Common patterns that produce false positives ─────────────────────
      // Initialized-then-reassigned pattern (let x = default; if (...) x = other)
      // is valid and common — disable to avoid false positives
      'no-useless-assignment': 'off',

      // ── Async safety ────────────────────────────────────────────────────
      // Catches unhandled promise rejections that cause Error #306
      '@typescript-eslint/no-floating-promises': 'off', // too noisy without strictNullChecks tuning

      // Allow empty catch blocks (common in this codebase)
      '@typescript-eslint/no-empty-object-type': 'warn',

      // preserve-caught-error requires `cause` chaining on every re-throw —
      // triggers across 40+ data adapters that use throw new Error(msg) legitimately
      'preserve-caught-error': 'off',

      // ── General safety ───────────────────────────────────────────────────
      'no-console': 'off',       // Console is used for debug logging
      'no-debugger': 'error',
      'no-undef': 'off',         // TypeScript handles this
    },
  },
);
