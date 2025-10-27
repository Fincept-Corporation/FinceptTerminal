// Encryption Service - Secure credential storage

/**
 * Simple encryption/decryption service using Web Crypto API
 * Stores a random key in localStorage (can be enhanced with user password)
 */
class EncryptionService {
  private static readonly STORAGE_KEY = 'data_mapping_encryption_key';
  private static readonly PREFERENCE_KEY = 'data_mapping_encryption_enabled';
  private cryptoKey: CryptoKey | null = null;

  /**
   * Initialize the encryption service
   */
  async initialize(): Promise<void> {
    const enabled = await this.isEncryptionEnabled();
    if (!enabled) return;

    // Get or generate encryption key
    let keyData = localStorage.getItem(EncryptionService.STORAGE_KEY);

    if (!keyData) {
      // Generate new key
      const key = await window.crypto.subtle.generateKey(
        {
          name: 'AES-GCM',
          length: 256,
        },
        true,
        ['encrypt', 'decrypt']
      );

      // Export and store key
      const exported = await window.crypto.subtle.exportKey('jwk', key);
      localStorage.setItem(EncryptionService.STORAGE_KEY, JSON.stringify(exported));

      this.cryptoKey = key;
    } else {
      // Import existing key
      const keyObject = JSON.parse(keyData);
      this.cryptoKey = await window.crypto.subtle.importKey(
        'jwk',
        keyObject,
        {
          name: 'AES-GCM',
          length: 256,
        },
        true,
        ['encrypt', 'decrypt']
      );
    }
  }

  /**
   * Encrypt a plaintext string
   */
  async encrypt(plaintext: string): Promise<string> {
    const enabled = await this.isEncryptionEnabled();
    if (!enabled) {
      // Return plaintext if encryption is disabled
      return plaintext;
    }

    if (!this.cryptoKey) {
      await this.initialize();
    }

    if (!this.cryptoKey) {
      throw new Error('Encryption key not initialized');
    }

    // Generate random IV
    const iv = window.crypto.getRandomValues(new Uint8Array(12));

    // Encrypt
    const encoder = new TextEncoder();
    const data = encoder.encode(plaintext);

    const encrypted = await window.crypto.subtle.encrypt(
      {
        name: 'AES-GCM',
        iv: iv,
      },
      this.cryptoKey,
      data
    );

    // Combine IV and encrypted data
    const combined = new Uint8Array(iv.length + encrypted.byteLength);
    combined.set(iv);
    combined.set(new Uint8Array(encrypted), iv.length);

    // Convert to base64
    return btoa(String.fromCharCode(...combined));
  }

  /**
   * Decrypt a ciphertext string
   */
  async decrypt(ciphertext: string): Promise<string> {
    const enabled = await this.isEncryptionEnabled();
    if (!enabled) {
      // Return as-is if encryption is disabled
      return ciphertext;
    }

    if (!this.cryptoKey) {
      await this.initialize();
    }

    if (!this.cryptoKey) {
      throw new Error('Encryption key not initialized');
    }

    try {
      // Decode from base64
      const combined = Uint8Array.from(atob(ciphertext), c => c.charCodeAt(0));

      // Extract IV and encrypted data
      const iv = combined.slice(0, 12);
      const encrypted = combined.slice(12);

      // Decrypt
      const decrypted = await window.crypto.subtle.decrypt(
        {
          name: 'AES-GCM',
          iv: iv,
        },
        this.cryptoKey,
        encrypted
      );

      // Convert to string
      const decoder = new TextDecoder();
      return decoder.decode(decrypted);
    } catch (error) {
      console.error('Decryption failed:', error);
      // Return ciphertext if decryption fails (might be unencrypted legacy data)
      return ciphertext;
    }
  }

  /**
   * Check if encryption is enabled
   */
  async isEncryptionEnabled(): Promise<boolean> {
    const preference = localStorage.getItem(EncryptionService.PREFERENCE_KEY);
    return preference === 'true' || preference === null; // Default to enabled
  }

  /**
   * Enable or disable encryption
   */
  async setEncryptionEnabled(enabled: boolean): Promise<void> {
    localStorage.setItem(EncryptionService.PREFERENCE_KEY, String(enabled));

    if (enabled) {
      await this.initialize();
    } else {
      this.cryptoKey = null;
    }
  }

  /**
   * Re-encrypt all stored credentials (when toggling encryption)
   */
  async reEncryptCredentials(
    credentials: Record<string, string>,
    enable: boolean
  ): Promise<Record<string, string>> {
    const result: Record<string, string> = {};

    for (const [key, value] of Object.entries(credentials)) {
      if (enable) {
        // Encrypt plaintext
        result[key] = await this.encrypt(value);
      } else {
        // Decrypt ciphertext
        result[key] = await this.decrypt(value);
      }
    }

    return result;
  }

  /**
   * Clear encryption key (useful for reset/logout)
   */
  clearKey(): void {
    localStorage.removeItem(EncryptionService.STORAGE_KEY);
    this.cryptoKey = null;
  }
}

// Export singleton instance
export const encryptionService = new EncryptionService();
export default encryptionService;
