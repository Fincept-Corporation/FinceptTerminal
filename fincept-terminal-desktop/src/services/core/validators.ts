/**
 * Input Validators - Production-ready validation utilities
 *
 * Features:
 * - Symbol validation (stocks, forex, crypto)
 * - Numeric validation
 * - String sanitization
 * - Batch validation
 *
 * Usage:
 *   import { validateSymbol, validateSymbolList, sanitizeInput } from '@/services/core/validators';
 */

// ============================================================================
// Types
// ============================================================================

export interface ValidationResult {
  valid: boolean;
  error?: string;
}

export interface ListValidationResult<T> {
  valid: boolean;
  error?: string;
  sanitized: T[];
  invalid: Array<{ value: T; error: string }>;
}

// ============================================================================
// Symbol Validation
// ============================================================================

// Pattern for valid ticker symbols (stocks, indices, forex, crypto)
// Allows: A-Z, 0-9, dots (.), hyphens (-), carets (^), equals (=)
const VALID_SYMBOL_REGEX = /^[A-Z0-9.\-^=]+$/i;
const MAX_SYMBOL_LENGTH = 20;
const MAX_SYMBOLS_PER_LIST = 100;

/**
 * Validate a single ticker symbol
 */
export function validateSymbol(symbol: unknown): ValidationResult {
  if (symbol === null || symbol === undefined) {
    return { valid: false, error: 'Symbol is required' };
  }

  if (typeof symbol !== 'string') {
    return { valid: false, error: 'Symbol must be a string' };
  }

  const trimmed = symbol.trim();

  if (trimmed.length === 0) {
    return { valid: false, error: 'Symbol cannot be empty' };
  }

  if (trimmed.length > MAX_SYMBOL_LENGTH) {
    return { valid: false, error: `Symbol too long (max ${MAX_SYMBOL_LENGTH} chars)` };
  }

  if (!VALID_SYMBOL_REGEX.test(trimmed)) {
    return { valid: false, error: 'Invalid characters in symbol' };
  }

  return { valid: true };
}

/**
 * Sanitize a symbol (uppercase, trim)
 */
export function sanitizeSymbol(symbol: string): string {
  return symbol.trim().toUpperCase();
}

/**
 * Validate and sanitize a list of symbols
 */
export function validateSymbolList(
  symbols: unknown,
  maxCount = MAX_SYMBOLS_PER_LIST
): ListValidationResult<string> {
  if (!Array.isArray(symbols)) {
    return {
      valid: false,
      error: 'Symbols must be an array',
      sanitized: [],
      invalid: [],
    };
  }

  if (symbols.length > maxCount) {
    return {
      valid: false,
      error: `Too many symbols (max ${maxCount})`,
      sanitized: [],
      invalid: [],
    };
  }

  const sanitized: string[] = [];
  const invalid: Array<{ value: string; error: string }> = [];

  for (const symbol of symbols) {
    const result = validateSymbol(symbol);
    if (result.valid) {
      sanitized.push(sanitizeSymbol(symbol as string));
    } else {
      invalid.push({ value: String(symbol), error: result.error! });
    }
  }

  return {
    valid: invalid.length === 0,
    error: invalid.length > 0 ? `${invalid.length} invalid symbol(s)` : undefined,
    sanitized,
    invalid,
  };
}

// ============================================================================
// Numeric Validation
// ============================================================================

export interface NumericValidationOptions {
  min?: number;
  max?: number;
  integer?: boolean;
  positive?: boolean;
  allowZero?: boolean;
}

/**
 * Validate a numeric value
 */
export function validateNumber(
  value: unknown,
  options: NumericValidationOptions = {}
): ValidationResult {
  const { min, max, integer = false, positive = false, allowZero = true } = options;

  if (value === null || value === undefined) {
    return { valid: false, error: 'Value is required' };
  }

  const num = typeof value === 'string' ? parseFloat(value) : value;

  if (typeof num !== 'number' || isNaN(num)) {
    return { valid: false, error: 'Value must be a number' };
  }

  if (!isFinite(num)) {
    return { valid: false, error: 'Value must be finite' };
  }

  if (integer && !Number.isInteger(num)) {
    return { valid: false, error: 'Value must be an integer' };
  }

  if (positive && num < 0) {
    return { valid: false, error: 'Value must be positive' };
  }

  if (!allowZero && num === 0) {
    return { valid: false, error: 'Value cannot be zero' };
  }

  if (min !== undefined && num < min) {
    return { valid: false, error: `Value must be at least ${min}` };
  }

  if (max !== undefined && num > max) {
    return { valid: false, error: `Value must be at most ${max}` };
  }

  return { valid: true };
}

// ============================================================================
// String Validation & Sanitization
// ============================================================================

export interface StringValidationOptions {
  minLength?: number;
  maxLength?: number;
  pattern?: RegExp;
  trim?: boolean;
}

/**
 * Validate a string value
 */
export function validateString(
  value: unknown,
  options: StringValidationOptions = {}
): ValidationResult {
  const { minLength = 0, maxLength = Infinity, pattern, trim = true } = options;

  if (value === null || value === undefined) {
    return { valid: false, error: 'Value is required' };
  }

  if (typeof value !== 'string') {
    return { valid: false, error: 'Value must be a string' };
  }

  const str = trim ? value.trim() : value;

  if (str.length < minLength) {
    return { valid: false, error: `Value must be at least ${minLength} characters` };
  }

  if (str.length > maxLength) {
    return { valid: false, error: `Value must be at most ${maxLength} characters` };
  }

  if (pattern && !pattern.test(str)) {
    return { valid: false, error: 'Value contains invalid characters' };
  }

  return { valid: true };
}

/**
 * Sanitize string input (trim, remove dangerous characters)
 */
export function sanitizeInput(input: string): string {
  return input
    .trim()
    .replace(/[<>]/g, '') // Remove potential HTML/script tags
    .replace(/[\x00-\x1F\x7F]/g, ''); // Remove control characters
}

// ============================================================================
// Email Validation
// ============================================================================

const EMAIL_REGEX = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

/**
 * Validate an email address
 */
export function validateEmail(email: unknown): ValidationResult {
  const strResult = validateString(email, { maxLength: 254 });
  if (!strResult.valid) return strResult;

  if (!EMAIL_REGEX.test(email as string)) {
    return { valid: false, error: 'Invalid email format' };
  }

  return { valid: true };
}

// ============================================================================
// Date Validation
// ============================================================================

/**
 * Validate a date value
 */
export function validateDate(value: unknown): ValidationResult {
  if (value === null || value === undefined) {
    return { valid: false, error: 'Date is required' };
  }

  let date: Date;

  if (value instanceof Date) {
    date = value;
  } else if (typeof value === 'string' || typeof value === 'number') {
    date = new Date(value);
  } else {
    return { valid: false, error: 'Invalid date format' };
  }

  if (isNaN(date.getTime())) {
    return { valid: false, error: 'Invalid date' };
  }

  return { valid: true };
}

/**
 * Validate a date range
 */
export function validateDateRange(
  startDate: unknown,
  endDate: unknown
): ValidationResult {
  const startResult = validateDate(startDate);
  if (!startResult.valid) return { valid: false, error: `Start date: ${startResult.error}` };

  const endResult = validateDate(endDate);
  if (!endResult.valid) return { valid: false, error: `End date: ${endResult.error}` };

  const start = new Date(startDate as string | number | Date);
  const end = new Date(endDate as string | number | Date);

  if (start > end) {
    return { valid: false, error: 'Start date must be before end date' };
  }

  return { valid: true };
}

