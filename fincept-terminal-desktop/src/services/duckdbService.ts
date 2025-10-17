import * as duckdb from 'duckdb';
import path from 'path';
import { app } from '@tauri-apps/api';

let db: duckdb.Database | null = null;

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
  id?: number;
  setting_key: string;
  setting_value: string;
  updated_at?: string;
}

// Initialize DuckDB database
export async function initializeDatabase(): Promise<void> {
  return new Promise(async (resolve, reject) => {
    try {
      // Get app data directory
      let dbPath: string;
      try {
        const appDataDir = await app.appDataDir();
        dbPath = path.join(appDataDir, 'fincept_terminal.duckdb');
      } catch (err) {
        // Fallback for development
        dbPath = './fincept_terminal.duckdb';
      }

      db = new duckdb.Database(dbPath, (err) => {
        if (err) {
          console.error('DuckDB initialization error:', err);
          reject(err);
          return;
        }

        // Create tables
        const connection = db!.connect();

        connection.all(`
          CREATE TABLE IF NOT EXISTS credentials (
            id INTEGER PRIMARY KEY,
            service_name VARCHAR NOT NULL,
            username VARCHAR NOT NULL,
            password VARCHAR NOT NULL,
            api_key VARCHAR,
            api_secret VARCHAR,
            additional_data TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
          )
        `, (err) => {
          if (err) {
            console.error('Error creating credentials table:', err);
            reject(err);
            return;
          }

          connection.all(`
            CREATE TABLE IF NOT EXISTS user_settings (
              id INTEGER PRIMARY KEY,
              setting_key VARCHAR UNIQUE NOT NULL,
              setting_value TEXT NOT NULL,
              updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
          `, (err) => {
            if (err) {
              console.error('Error creating user_settings table:', err);
              reject(err);
              return;
            }

            console.log('DuckDB initialized successfully at:', dbPath);
            resolve();
          });
        });
      });
    } catch (error) {
      console.error('Failed to initialize DuckDB:', error);
      reject(error);
    }
  });
}

// Credential Management Functions

export async function saveCredential(credential: Credential): Promise<{ success: boolean; message: string; id?: number }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    const connection = db.connect();

    // Check if credential already exists
    connection.all(
      'SELECT id FROM credentials WHERE service_name = ? AND username = ?',
      [credential.service_name, credential.username],
      (err, rows: any[]) => {
        if (err) {
          resolve({ success: false, message: `Error checking credential: ${err.message}` });
          return;
        }

        if (rows && rows.length > 0) {
          // Update existing credential
          connection.run(
            `UPDATE credentials
             SET password = ?, api_key = ?, api_secret = ?, additional_data = ?, updated_at = CURRENT_TIMESTAMP
             WHERE id = ?`,
            [credential.password, credential.api_key || null, credential.api_secret || null, credential.additional_data || null, rows[0].id],
            (err) => {
              if (err) {
                resolve({ success: false, message: `Error updating credential: ${err.message}` });
              } else {
                resolve({ success: true, message: 'Credential updated successfully', id: rows[0].id });
              }
            }
          );
        } else {
          // Insert new credential
          connection.run(
            `INSERT INTO credentials (service_name, username, password, api_key, api_secret, additional_data)
             VALUES (?, ?, ?, ?, ?, ?)`,
            [credential.service_name, credential.username, credential.password, credential.api_key || null, credential.api_secret || null, credential.additional_data || null],
            function(err) {
              if (err) {
                resolve({ success: false, message: `Error saving credential: ${err.message}` });
              } else {
                resolve({ success: true, message: 'Credential saved successfully', id: this.lastID });
              }
            }
          );
        }
      }
    );
  });
}

export async function getCredentials(): Promise<{ success: boolean; data?: Credential[]; message?: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    const connection = db.connect();
    connection.all('SELECT * FROM credentials ORDER BY created_at DESC', (err, rows: any[]) => {
      if (err) {
        resolve({ success: false, message: `Error fetching credentials: ${err.message}` });
      } else {
        resolve({ success: true, data: rows });
      }
    });
  });
}

export async function getCredentialByService(serviceName: string): Promise<{ success: boolean; data?: Credential; message?: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    const connection = db.connect();
    connection.all('SELECT * FROM credentials WHERE service_name = ? LIMIT 1', [serviceName], (err, rows: any[]) => {
      if (err) {
        resolve({ success: false, message: `Error fetching credential: ${err.message}` });
      } else if (rows && rows.length > 0) {
        resolve({ success: true, data: rows[0] });
      } else {
        resolve({ success: false, message: 'Credential not found' });
      }
    });
  });
}

export async function deleteCredential(id: number): Promise<{ success: boolean; message: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    const connection = db.connect();
    connection.run('DELETE FROM credentials WHERE id = ?', [id], (err) => {
      if (err) {
        resolve({ success: false, message: `Error deleting credential: ${err.message}` });
      } else {
        resolve({ success: true, message: 'Credential deleted successfully' });
      }
    });
  });
}

// Settings Management Functions

export async function saveSetting(key: string, value: string): Promise<{ success: boolean; message: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    const connection = db.connect();

    connection.run(
      `INSERT INTO user_settings (setting_key, setting_value)
       VALUES (?, ?)
       ON CONFLICT (setting_key) DO UPDATE SET setting_value = ?, updated_at = CURRENT_TIMESTAMP`,
      [key, value, value],
      (err) => {
        if (err) {
          resolve({ success: false, message: `Error saving setting: ${err.message}` });
        } else {
          resolve({ success: true, message: 'Setting saved successfully' });
        }
      }
    );
  });
}

export async function getSetting(key: string): Promise<{ success: boolean; data?: string; message?: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    const connection = db.connect();
    connection.all('SELECT setting_value FROM user_settings WHERE setting_key = ?', [key], (err, rows: any[]) => {
      if (err) {
        resolve({ success: false, message: `Error fetching setting: ${err.message}` });
      } else if (rows && rows.length > 0) {
        resolve({ success: true, data: rows[0].setting_value });
      } else {
        resolve({ success: false, message: 'Setting not found' });
      }
    });
  });
}

export async function getAllSettings(): Promise<{ success: boolean; data?: Record<string, string>; message?: string }> {
  return new Promise((resolve) => {
    if (!db) {
      resolve({ success: false, message: 'Database not initialized' });
      return;
    }

    const connection = db.connect();
    connection.all('SELECT setting_key, setting_value FROM user_settings', (err, rows: any[]) => {
      if (err) {
        resolve({ success: false, message: `Error fetching settings: ${err.message}` });
      } else {
        const settings: Record<string, string> = {};
        rows.forEach(row => {
          settings[row.setting_key] = row.setting_value;
        });
        resolve({ success: true, data: settings });
      }
    });
  });
}

export async function closeDatabase(): Promise<void> {
  return new Promise((resolve) => {
    if (db) {
      db.close((err) => {
        if (err) {
          console.error('Error closing database:', err);
        }
        db = null;
        resolve();
      });
    } else {
      resolve();
    }
  });
}