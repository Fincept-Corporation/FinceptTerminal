/**
 * Translation Service
 * Uses Python googletrans library via execute_python_script
 * With in-memory caching for improved performance
 */

import { invoke } from '@tauri-apps/api/core';

interface TranslationResponse {
  success: boolean;
  translated_text?: string;
  detected_lang?: string;
  error?: string;
}

interface BatchTranslationItem {
  original: string;
  translated: string;
  detected_lang: string;
}

interface BatchTranslationResponse {
  success: boolean;
  translations?: BatchTranslationItem[];
  count: number;
  error?: string;
}

// In-memory translation cache
class TranslationCache {
  private cache: Map<string, string> = new Map();
  private maxSize: number = 1000; // Limit cache size

  private getCacheKey(text: string, sourceLang: string, targetLang: string): string {
    return `${sourceLang}:${targetLang}:${text}`;
  }

  get(text: string, sourceLang: string, targetLang: string): string | undefined {
    return this.cache.get(this.getCacheKey(text, sourceLang, targetLang));
  }

  set(text: string, sourceLang: string, targetLang: string, translation: string): void {
    // Simple LRU: if cache is full, delete oldest entry
    if (this.cache.size >= this.maxSize) {
      const firstKey = this.cache.keys().next().value;
      if (firstKey) {
        this.cache.delete(firstKey);
      }
    }
    this.cache.set(this.getCacheKey(text, sourceLang, targetLang), translation);
  }

  clear(): void {
    this.cache.clear();
  }
}

const translationCache = new TranslationCache();

export class TranslationService {
  /**
   * Translate a single text (with caching)
   */
  static async translateText(
    text: string,
    sourceLang: string = 'auto',
    targetLang: string = 'en'
  ): Promise<string> {
    if (!text || text.trim() === '') return '';

    // Check cache first
    const cached = translationCache.get(text, sourceLang, targetLang);
    if (cached) {
      return cached;
    }

    try {
      const result = await invoke<string>('execute_python_script', {
        scriptName: 'translate_text.py',
        args: ['single', text, sourceLang, targetLang],
        env: {},
      });

      const response: TranslationResponse = JSON.parse(result);

      if (response.success && response.translated_text) {
        // Cache the translation
        translationCache.set(text, sourceLang, targetLang, response.translated_text);
        return response.translated_text;
      } else {
        console.error('[Translation] Error:', response.error);
        return text; // Return original text on error
      }
    } catch (error) {
      console.error('[Translation] Failed to translate:', error);
      return text; // Return original text on error
    }
  }

  /**
   * Translate Chinese text to English
   */
  static async translateToEnglish(text: string | null | undefined): Promise<string> {
    if (!text) return '';

    // If text doesn't contain Chinese characters, return as is
    if (!this.isChinese(text)) return text;

    return await this.translateText(text, 'zh-cn', 'en');
  }

  /**
   * Translate multiple texts in batch (with caching and optimization)
   */
  static async translateBatch(
    texts: string[],
    sourceLang: string = 'auto',
    targetLang: string = 'en'
  ): Promise<string[]> {
    if (!texts || texts.length === 0) return [];

    // Separate cached and uncached texts
    const results: string[] = new Array(texts.length);
    const uncachedIndices: number[] = [];
    const uncachedTexts: string[] = [];

    texts.forEach((text, index) => {
      const cached = translationCache.get(text, sourceLang, targetLang);
      if (cached) {
        results[index] = cached;
      } else {
        uncachedIndices.push(index);
        uncachedTexts.push(text);
      }
    });

    // If all translations are cached, return immediately
    if (uncachedTexts.length === 0) {
      console.log('[Translation] All translations served from cache');
      return results;
    }

    console.log(`[Translation] ${uncachedTexts.length}/${texts.length} texts need translation`);

    try {
      const result = await invoke<string>('execute_python_script', {
        scriptName: 'translate_text.py',
        args: ['batch', JSON.stringify(uncachedTexts), sourceLang, targetLang],
        env: {},
      });

      const response: BatchTranslationResponse = JSON.parse(result);

      if (response.success && response.translations) {
        // Fill in the uncached results and cache them
        response.translations.forEach((item, i) => {
          const originalIndex = uncachedIndices[i];
          results[originalIndex] = item.translated;
          // Cache the translation
          translationCache.set(item.original, sourceLang, targetLang, item.translated);
        });
        return results;
      } else {
        console.error('[Translation] Batch error:', response.error);
        // Fill uncached positions with original texts
        uncachedIndices.forEach((index, i) => {
          results[index] = uncachedTexts[i];
        });
        return results;
      }
    } catch (error) {
      console.error('[Translation] Failed to translate batch:', error);
      // Fill uncached positions with original texts
      uncachedIndices.forEach((index, i) => {
        results[index] = uncachedTexts[i];
      });
      return results;
    }
  }

  /**
   * Translate object properties
   */
  static async translateObject<T extends Record<string, any>>(
    obj: T,
    fieldsToTranslate: (keyof T)[],
    sourceLang: string = 'auto',
    targetLang: string = 'en'
  ): Promise<T> {
    const translatedObj = { ...obj };

    // Collect texts to translate
    const textsToTranslate: string[] = [];
    const fieldIndices: { [key: number]: keyof T } = {};

    fieldsToTranslate.forEach((field, index) => {
      const value = obj[field];
      if (typeof value === 'string' && value.trim() !== '') {
        textsToTranslate.push(value);
        fieldIndices[textsToTranslate.length - 1] = field;
      }
    });

    if (textsToTranslate.length === 0) {
      return translatedObj;
    }

    // Translate in batch
    const translations = await this.translateBatch(textsToTranslate, sourceLang, targetLang);

    // Apply translations back to object
    translations.forEach((translation, index) => {
      const field = fieldIndices[index];
      if (field) {
        translatedObj[field] = translation as T[keyof T];
      }
    });

    return translatedObj;
  }

  /**
   * Translate batch with key-value pairs (for AsiaMarketsTab compatibility)
   */
  static async translateBatchWithKeys(
    items: { key: string; text: string }[],
    sourceLang: string = 'auto',
    targetLang: string = 'en'
  ): Promise<Record<string, string>> {
    const result: Record<string, string> = {};

    if (!items || items.length === 0) return result;

    // Extract texts
    const texts = items.map(item => item.text);

    // Translate
    const translations = await this.translateBatch(texts, sourceLang, targetLang);

    // Map back to keys
    items.forEach((item, index) => {
      result[item.key] = translations[index] || item.text;
    });

    return result;
  }

  /**
   * Check if text contains Chinese characters
   */
  static isChinese(text: string): boolean {
    const chineseRegex = /[\u4e00-\u9fa5]/;
    return chineseRegex.test(text);
  }

  /**
   * Clear the translation cache
   */
  static clearCache(): void {
    translationCache.clear();
    console.log('[Translation] Cache cleared');
  }

  /**
   * Get cache statistics
   */
  static getCacheStats(): { size: number; maxSize: number } {
    return {
      size: (translationCache as any).cache.size,
      maxSize: (translationCache as any).maxSize
    };
  }
}
