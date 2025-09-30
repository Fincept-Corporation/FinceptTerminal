// Browser-compatible storage service using IndexedDB
// This replaces DuckDB for credential storage in the browser environment

interface Credential {
  id?: number;
  service_name: string;
  username: string;
  password: string;
  api_key?: string;
  api_secret?: string;
  additional_data?: string;
  created_at?: string;
  updated_at?: string;
}

interface UserSettings {
  setting_key: string;
  setting_value: string;
  updated_at?: string;
}

const DB_NAME = 'fincept_terminal_db';
const DB_VERSION = 1;
const CREDENTIALS_STORE = 'credentials';
const SETTINGS_STORE = 'settings';

let db: IDBDatabase | null = null;

// Initialize IndexedDB
export async function initializeDatabase(): Promise<void> {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DB_NAME, DB_VERSION);

    request.onerror = () => {
      console.error('IndexedDB initialization error:', request.error);
      reject(request.error);
    };

    request.onsuccess = () => {
      db = request.result;
      console.log('IndexedDB initialized successfully');
      resolve();
    };

    request.onupgradeneeded = (event) => {
      const database = (event.target as IDBOpenDBRequest).result;

      // Create credentials store
      if (!database.objectStoreNames.contains(CREDENTIALS_STORE)) {
        const credStore = database.createObjectStore(CREDENTIALS_STORE, {
          keyPath: 'id',
          autoIncrement: true
        });
        credStore.createIndex('service_username', ['service_name', 'username'], { unique: true });
        credStore.createIndex('service_name', 'service_name', { unique: false });
      }

      // Create settings store
      if (!database.objectStoreNames.contains(SETTINGS_STORE)) {
        const settingsStore = database.createObjectStore(SETTINGS_STORE, {
          keyPath: 'setting_key'
        });
      }

      console.log('IndexedDB schema created');
    };
  });
}

// Credential Management Functions

export async function saveCredential(credential: Credential): Promise<{ success: boolean; message: string; id?: number }> {
  return new Promise(async (resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    try {
      const transaction = db.transaction([CREDENTIALS_STORE], 'readwrite');
      const store = transaction.objectStore(CREDENTIALS_STORE);
      const index = store.index('service_username');

      // Check if credential exists
      const checkRequest = index.get([credential.service_name, credential.username]);

      checkRequest.onsuccess = () => {
        const existingCred = checkRequest.result;

        const credentialToSave = {
          ...credential,
          updated_at: new Date().toISOString()
        };

        if (existingCred) {
          // Update existing
          credentialToSave.id = existingCred.id;
          credentialToSave.created_at = existingCred.created_at;

          const updateRequest = store.put(credentialToSave);

          updateRequest.onsuccess = () => {
            resolve({ success: true, message: 'Credential updated successfully', id: existingCred.id });
          };

          updateRequest.onerror = () => {
            resolve({ success: false, message: `Error updating credential: ${updateRequest.error?.message}` });
          };
        } else {
          // Insert new
          credentialToSave.created_at = new Date().toISOString();

          const addRequest = store.add(credentialToSave);

          addRequest.onsuccess = () => {
            resolve({ success: true, message: 'Credential saved successfully', id: addRequest.result as number });
          };

          addRequest.onerror = () => {
            resolve({ success: false, message: `Error saving credential: ${addRequest.error?.message}` });
          };
        }
      };

      checkRequest.onerror = () => {
        resolve({ success: false, message: `Error checking credential: ${checkRequest.error?.message}` });
      };
    } catch (error: any) {
      resolve({ success: false, message: `Error: ${error.message}` });
    }
  });
}

export async function getCredentials(): Promise<{ success: boolean; data?: Credential[]; message?: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    try {
      const transaction = db.transaction([CREDENTIALS_STORE], 'readonly');
      const store = transaction.objectStore(CREDENTIALS_STORE);
      const request = store.getAll();

      request.onsuccess = () => {
        const credentials = request.result as Credential[];
        // Sort by created_at desc
        credentials.sort((a, b) => {
          const dateA = new Date(a.created_at || 0).getTime();
          const dateB = new Date(b.created_at || 0).getTime();
          return dateB - dateA;
        });
        resolve({ success: true, data: credentials });
      };

      request.onerror = () => {
        resolve({ success: false, message: `Error fetching credentials: ${request.error?.message}` });
      };
    } catch (error: any) {
      resolve({ success: false, message: `Error: ${error.message}` });
    }
  });
}

export async function getCredentialByService(serviceName: string): Promise<{ success: boolean; data?: Credential; message?: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    try {
      const transaction = db.transaction([CREDENTIALS_STORE], 'readonly');
      const store = transaction.objectStore(CREDENTIALS_STORE);
      const index = store.index('service_name');
      const request = index.get(serviceName);

      request.onsuccess = () => {
        const credential = request.result;
        if (credential) {
          resolve({ success: true, data: credential });
        } else {
          resolve({ success: false, message: 'Credential not found' });
        }
      };

      request.onerror = () => {
        resolve({ success: false, message: `Error fetching credential: ${request.error?.message}` });
      };
    } catch (error: any) {
      resolve({ success: false, message: `Error: ${error.message}` });
    }
  });
}

export async function deleteCredential(id: number): Promise<{ success: boolean; message: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    try {
      const transaction = db.transaction([CREDENTIALS_STORE], 'readwrite');
      const store = transaction.objectStore(CREDENTIALS_STORE);
      const request = store.delete(id);

      request.onsuccess = () => {
        resolve({ success: true, message: 'Credential deleted successfully' });
      };

      request.onerror = () => {
        resolve({ success: false, message: `Error deleting credential: ${request.error?.message}` });
      };
    } catch (error: any) {
      resolve({ success: false, message: `Error: ${error.message}` });
    }
  });
}

// Settings Management Functions

export async function saveSetting(key: string, value: string): Promise<{ success: boolean; message: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    try {
      const transaction = db.transaction([SETTINGS_STORE], 'readwrite');
      const store = transaction.objectStore(SETTINGS_STORE);

      const setting: UserSettings = {
        setting_key: key,
        setting_value: value,
        updated_at: new Date().toISOString()
      };

      const request = store.put(setting);

      request.onsuccess = () => {
        resolve({ success: true, message: 'Setting saved successfully' });
      };

      request.onerror = () => {
        resolve({ success: false, message: `Error saving setting: ${request.error?.message}` });
      };
    } catch (error: any) {
      resolve({ success: false, message: `Error: ${error.message}` });
    }
  });
}

export async function getSetting(key: string): Promise<{ success: boolean; data?: string; message?: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    try {
      const transaction = db.transaction([SETTINGS_STORE], 'readonly');
      const store = transaction.objectStore(SETTINGS_STORE);
      const request = store.get(key);

      request.onsuccess = () => {
        const setting = request.result as UserSettings;
        if (setting) {
          resolve({ success: true, data: setting.setting_value });
        } else {
          resolve({ success: false, message: 'Setting not found' });
        }
      };

      request.onerror = () => {
        resolve({ success: false, message: `Error fetching setting: ${request.error?.message}` });
      };
    } catch (error: any) {
      resolve({ success: false, message: `Error: ${error.message}` });
    }
  });
}

export async function getAllSettings(): Promise<{ success: boolean; data?: Record<string, string>; message?: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    try {
      const transaction = db.transaction([SETTINGS_STORE], 'readonly');
      const store = transaction.objectStore(SETTINGS_STORE);
      const request = store.getAll();

      request.onsuccess = () => {
        const settings: Record<string, string> = {};
        const allSettings = request.result as UserSettings[];

        allSettings.forEach(setting => {
          settings[setting.setting_key] = setting.setting_value;
        });

        resolve({ success: true, data: settings });
      };

      request.onerror = () => {
        resolve({ success: false, message: `Error fetching settings: ${request.error?.message}` });
      };
    } catch (error: any) {
      resolve({ success: false, message: `Error: ${error.message}` });
    }
  });
}

export async function closeDatabase(): Promise<void> {
  return new Promise((resolve) => {
    if (db) {
      db.close();
      db = null;
    }
    resolve();
  });
}